#include "pch.h"
#include "SequenceRunner.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  const char* const _timeoutMessage = "The sequence did not complete within its timeout.";

  struct SequenceRunner::RunState
  {
    uint32_t Sequence = 0;
    steady_clock::time_point Deadline;
    CancellationToken Stop;
    uint32_t InlineCaptures = 0;
    atomic_bool IsInterrupted = false;

    //Guarded by SequenceRunner::_mutex
    unordered_set<uint64_t> PendingAsync;
    value_ptr<ActionResult> AsyncFailure;
  };

  namespace
  {
    milliseconds Remaining(steady_clock::time_point deadline)
    {
      return max(duration_cast<milliseconds>(deadline - steady_clock::now()), milliseconds(0));
    }

    bool IsHeld(const InputState& state)
    {
      return !state.MouseButtons.empty() || !state.Keys.empty() || !state.TouchPoints.empty();
    }
  }

  SequenceRunner::SequenceRunner(dependency_container* container) :
    RunningChanged(_events),
    _driver(container->resolve<IDriverLink>()),
    _actionCompletedSubscription(_driver->ActionCompleted({ this, &SequenceRunner::OnActionCompleted })),
    _agentControl(container->resolve<AgentControl>()),
    _modeChangedSubscription(_agentControl->ModeChanged({ this, &SequenceRunner::OnModeChanged })),
    _captureStore(container->resolve<CaptureStore>()),
    _actionLog(container->resolve<ActionLog>()),
    _visualization(container->resolve<VisualizationFeed>())
  { }

  RunSequenceResponse SequenceRunner::Run(const RunSequenceRequest& request, const ProgressCallback& progress, const CancellationToken& cancellation)
  {
    if (!_driver->IsConnected()) return Failure(ErrorCode::SessionNotActive, "No session is running; start one with Initialize-Session.");
    if (!_agentControl->IsAgentAllowed()) return Failure(ErrorCode::AgentInputDisabled, RefusalMessage());

    unique_lock runLock{ _runMutex, try_to_lock };
    if (!runLock) return Failure(ErrorCode::ActionFailed, "Another sequence is running; sequences run one at a time.");

    //The state of this run, which async completions and the Stop button reach through _run
    RunState state;
    state.Sequence = _nextSequence++;
    state.Deadline = steady_clock::now() + (request.Timeout ? milliseconds(*request.Timeout) : RunSequenceArguments::DefaultTimeout);
    auto cancelledSubscription = cancellation.Cancelled([&](CancellationToken*) { state.Stop.Cancel(); });
    if (cancellation.IsCancelled()) state.Stop.Cancel();
    {
      lock_guard lock{ _mutex };
      _run = &state;

      //The user may have stopped the agent since the check above, before OnModeChanged could see this run
      if (!_agentControl->IsAgentAllowed())
      {
        state.IsInterrupted = true;
        state.Stop.Cancel();
      }
    }
    _events.raise(RunningChanged, this, true);

    RunSequenceResponse response;
    for (uint32_t index = 0; index < request.Steps.size(); index++)
    {
      auto& step = request.Steps[index];
      _actionLog->Start(step);
      auto visualId = _visualization->OnActionStarted(*step);
      auto result = RunStep(state, step, index);
      _visualization->OnActionFinished(visualId, *step, *result);
      _actionLog->Add(step, *result);

      if (result->Status != ActionStatus::Succeeded)
      {
        ReportFailure(response, state, index, *result);
        break;
      }

      response.Results.push_back(move(result));
      if (progress) progress(index + 1, uint32_t(request.Steps.size()), step->Name());
    }

    {
      lock_guard lock{ _mutex };
      _run = nullptr;
    }
    _events.raise(RunningChanged, this, false);

    //After a failure nothing stays held, and the AI learns what was
    if (response.FailedStep)
    {
      auto reset = _driver->ResetInput();
      if (auto released = get_if<ResetInputResponse>(&reset); released && IsHeld(released->Released)) response.ReleasedInput = released->Released;
      _visualization->OnInputReleased();
    }

    return response;
  }

  value_ptr<ActionResult> SequenceRunner::RunStep(RunState& state, const value_ptr<Action>& step, uint32_t index)
  {
    if (state.Stop.IsCancelled()) return ActionResult::Cancelled();
    if (Remaining(state.Deadline) == 0ms) return ActionResult::Failed(ErrorCode::Timeout, _timeoutMessage);

    switch (step->Type())
    {
    case ActionType::ControlWait:
      return Wait(state, milliseconds(static_cast<const ControlWaitAction&>(*step).Duration));
    case ActionType::ControlJoin:
      return Join(state);
    case ActionType::ControlReset:
      return Reset(state);
    case ActionType::CaptureScreen:
    case ActionType::CaptureWindow:
    case ActionType::CaptureRegion:
      return Capture(state, step, index);
    case ActionType::ClipboardRead:
      return ReadClipboard(state, step, index);
    default:
      return Execute(state, step);
    }
  }

  value_ptr<ActionResult> SequenceRunner::Wait(RunState& state, milliseconds duration)
  {
    auto remaining = Remaining(state.Deadline);
    if (state.Stop.WaitFor(min(duration, remaining))) return ActionResult::Cancelled();
    return duration > remaining ? ActionResult::Failed(ErrorCode::Timeout, _timeoutMessage) : ActionResult::Succeeded();
  }

  value_ptr<ActionResult> SequenceRunner::Join(RunState& state)
  {
    unique_lock lock{ _mutex };
    while (!state.PendingAsync.empty())
    {
      if (state.Stop.IsCancelled()) return ActionResult::Cancelled();
      if (Remaining(state.Deadline) == 0ms) return ActionResult::Failed(ErrorCode::Timeout, _timeoutMessage);
      _asyncChanged.wait_for(lock, min(Remaining(state.Deadline), milliseconds(100)));
    }

    //The first async action that failed fails the join
    if (state.AsyncFailure) return exchange(state.AsyncFailure, nullptr);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> SequenceRunner::Reset(RunState& state)
  {
    //The driver cancels its async actions too
    auto reset = _driver->ResetInput();
    if (auto error = get_if<JsonRpcError>(&reset)) return ActionResult::Failed(ProtocolErrors::TryGetCode(*error).value_or(ErrorCode::ActionFailed), error->Message);

    lock_guard lock{ _mutex };
    state.PendingAsync.clear();
    _asyncChanged.notify_all();
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> SequenceRunner::Capture(RunState& state, const value_ptr<Action>& step, uint32_t index)
  {
    //The driver gets the action with the settings' defaults, and a tag
    auto action = step;
    auto& capture = static_cast<CaptureAction&>(*action);
    if (!capture.Tag) capture.Tag = to_string(index);

    auto destination = _captureStore->Decide(capture.Output, state.InlineCaptures);
    _captureStore->ApplyDefaults(capture, destination);

    auto result = Execute(state, action);
    auto captureResult = dynamic_cast<CaptureResult*>(result.get());
    if (!captureResult) return result;

    auto& image = captureResult->Image;
    return StoreImage(state, move(result), image, capture.Path, destination, format("{}-{}-{}", state.Sequence, index, *capture.Tag));
  }

  value_ptr<ActionResult> SequenceRunner::ReadClipboard(RunState& state, const value_ptr<Action>& step, uint32_t index)
  {
    //An image on the clipboard is returned as a capture is
    auto action = step;
    auto& read = static_cast<ClipboardReadAction&>(*action);
    auto destination = _captureStore->Decide(read.Output, state.InlineCaptures);
    _captureStore->ApplyDefaults(read);

    auto result = Execute(state, action);
    auto clipboardResult = dynamic_cast<ClipboardResult*>(result.get());
    if (!clipboardResult || !clipboardResult->Image) return result;

    auto& image = *clipboardResult->Image;
    return StoreImage(state, move(result), image, read.Path, destination, format("{}-{}-clipboard", state.Sequence, index));
  }

  value_ptr<ActionResult> SequenceRunner::StoreImage(RunState& state, value_ptr<ActionResult>&& result, CapturedImage& image, const optional<string>& path, const CaptureDestination& destination, string_view fileStem)
  {
    try
    {
      _captureStore->Store(image, *result, path, destination, fileStem);
    }
    catch (const exception& exception)
    {
      return ActionResult::Failed(ErrorCode::ActionFailed, exception.what());
    }

    if (destination.IsInline) state.InlineCaptures++;
    return move(result);
  }

  value_ptr<ActionResult> SequenceRunner::Execute(RunState& state, const value_ptr<Action>& action)
  {
    auto actionId = _nextActionId++;
    if (action->IsAsync())
    {
      lock_guard lock{ _mutex };
      state.PendingAsync.insert(actionId);
    }

    auto response = _driver->Execute(ExecuteActionRequest{ actionId, action }, Remaining(state.Deadline), state.Stop);
    if (auto error = get_if<JsonRpcError>(&response))
    {
      {
        lock_guard lock{ _mutex };
        state.PendingAsync.erase(actionId);
      }

      if (error->Code == int32_t(JsonRpcErrorCode::RequestCancelled)) return ActionResult::Cancelled();
      if (error->Code == int32_t(JsonRpcErrorCode::Timeout)) return ActionResult::Failed(ErrorCode::Timeout, _timeoutMessage);
      return ActionResult::Failed(ProtocolErrors::TryGetCode(*error).value_or(ErrorCode::ActionFailed), error->Message);
    }

    auto& result = get<ExecuteActionResponse>(response).Result;
    return result ? result : ActionResult::Failed(ErrorCode::ActionFailed, "The driver returned no result.");
  }

  RunSequenceResponse SequenceRunner::Failure(ErrorCode error, string message) const
  {
    RunSequenceResponse response;
    response.Error = error;
    response.Message = move(message);
    return response;
  }

  string SequenceRunner::RefusalMessage() const
  {
    if (_agentControl->Mode() == ControlMode::User) return "The user has taken control of the session; sequences fail until they give it back.";
    return "The user stopped the agent; sequences fail until they give control back.";
  }

  void SequenceRunner::ReportFailure(RunSequenceResponse& response, const RunState& state, uint32_t index, const ActionResult& result) const
  {
    response.FailedStep = index;
    if (result.Status == ActionStatus::Failed)
    {
      response.Error = result.Error.value_or(ErrorCode::ActionFailed);
      response.Message = result.Message;
    }
    else if (state.IsInterrupted)
    {
      response.Error = ErrorCode::UserInterrupted;
      response.Message = RefusalMessage();
    }
    else if (Remaining(state.Deadline) == 0ms)
    {
      response.Error = ErrorCode::Timeout;
      response.Message = _timeoutMessage;
    }
    else
    {
      response.Error = ErrorCode::ActionFailed;
      response.Message = "The sequence was cancelled.";
    }
  }

  void SequenceRunner::OnActionCompleted(IDriverLink* /*sender*/, const ActionCompletedNotification& notification)
  {
    lock_guard lock{ _mutex };
    if (!_run || !_run->PendingAsync.erase(notification.ActionId)) return;

    if (!_run->AsyncFailure && notification.Result && notification.Result->Status != ActionStatus::Succeeded) _run->AsyncFailure = notification.Result;
    _asyncChanged.notify_all();
  }

  void SequenceRunner::OnModeChanged(AgentControl* /*sender*/, ControlMode mode)
  {
    if (mode == ControlMode::Agent) return;

    //A running sequence fails, and releases what is held when it does
    {
      lock_guard lock{ _mutex };
      if (_run)
      {
        _run->IsInterrupted = true;
        _run->Stop.Cancel();
        return;
      }
    }

    //Else the agent may still hold what an earlier sequence pressed
    if (!_driver->IsConnected()) return;
    _driver->ResetInput();
    _visualization->OnInputReleased();
  }
}
