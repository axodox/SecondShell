#include "pch.h"
#include "SequenceRunner.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "ProcessActions.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Threading;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  const char* const _timeoutMessage = "The sequence did not complete within its timeout.";

  //Input waits out the switch of the user's session to or from the connection that keeps it unlocked behind the lock
  //screen, about 11 s (Findings.md 2.17), rather than failing; longer than this, and the agent hears about it
  const milliseconds _inputResumeLimit{ 20000 };
  const milliseconds _inputPollInterval{ 250 };
  //Input refused just as the session locked: until the lock screen bridge reports that it is resuming
  const milliseconds _inputRefusalGrace{ 2000 };
  //Windows reports the session unlocked about 2 s before it takes input again when the user signs in at the PC
  const milliseconds _inputSettleTime{ 500 };
  const uint32_t _inputRetries = 6;

  struct SequenceRunner::RunState
  {
    uint32_t Sequence = 0;
    steady_clock::time_point Deadline;
    CancellationToken Stop;
    uint32_t InlineCaptures = 0;
    atomic_bool IsInterrupted = false;
    optional<filesystem::path> CaptureFile;  //of the step which just ran, for the log

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
      return !state.MouseButtons.empty() || !state.Keys.empty() || !state.TouchPoints.empty() || state.Pen.value_or(false);
    }

    bool IsInput(const Action& action)
    {
      auto kind = action.Kind();
      return kind == ActionKind::Mouse || kind == ActionKind::Keyboard || kind == ActionKind::Touch || kind == ActionKind::Pen;
    }

    //How long a step is expected to take, for its progress
    milliseconds ExpectedDuration(const Action& action, milliseconds remaining)
    {
      if (action.Type() == ActionType::ControlWait) return milliseconds(static_cast<const ControlWaitAction&>(action).Duration);
      return min(remaining, action.WaitTimeout().value_or(Action::DefaultWaitTimeout));
    }

    //Reports a sequence's progress: after each step, and, while a long step runs, again whenever nothing has been
    //reported for a while: just often enough that clients which reset their timeout on progress do not give up on it.
    //The values only increase, as MCP requires.
    class ProgressReporter
    {
    public:
      ProgressReporter(const ProgressCallback& callback, uint32_t total, milliseconds keepAliveInterval) :
        _callback(callback),
        _total(total),
        _keepAliveInterval(keepAliveInterval)
      {
        if (_callback) _thread = thread([this] { Run(); });
      }

      ~ProgressReporter()
      {
        if (!_thread.joinable()) return;
        {
          lock_guard lock{ _mutex };
          _isDone = true;
        }
        _wake.notify_all();
        _thread.join();
      }

      void StartStep(uint32_t index, const char* name, milliseconds expected)
      {
        lock_guard lock{ _mutex };
        _index = index;
        _name = name;
        _start = steady_clock::now();
        _expected = max(expected, 1ms);
      }

      void CompleteStep(uint32_t index, string_view name)
      {
        lock_guard lock{ _mutex };
        _name = nullptr;
        Report(index + 1, nullopt, name);
      }

    private:
      const ProgressCallback& _callback;
      uint32_t _total;
      milliseconds _keepAliveInterval;

      mutex _mutex;
      condition_variable _wake;
      bool _isDone = false;
      uint32_t _index = 0;
      const char* _name = nullptr;  //of the running step
      steady_clock::time_point _start;
      milliseconds _expected{ 1 };
      double _reported = 0;
      steady_clock::time_point _reportedAt = steady_clock::now();
      thread _thread;

      void Run()
      {
        set_thread_name("* Sequence progress");

        unique_lock lock{ _mutex };
        while (!_wake.wait_until(lock, _reportedAt + _keepAliveInterval, [this] { return _isDone; }))
        {
          //Reported meanwhile, or no step running
          auto now = steady_clock::now();
          if (now < _reportedAt + _keepAliveInterval) continue;
          if (!_name)
          {
            _reportedAt = now;
            continue;
          }

          //Up to 90% over the expected time, then ever closer to 100% for a step which takes longer
          auto elapsed = duration_cast<milliseconds>(now - _start);
          auto ratio = double(elapsed.count()) / double(_expected.count());
          auto fraction = ratio < 1 ? 0.9 * ratio : 1 - 0.1 / ratio;
          Report(_index, fraction, format("{}, running for {} s", _name, duration_cast<seconds>(elapsed).count()));
        }
      }

      //Under _mutex
      void Report(uint32_t completed, optional<double> step, string_view message)
      {
        auto value = completed + step.value_or(0);
        if (value <= _reported) return;

        _reported = value;
        _reportedAt = steady_clock::now();
        if (_callback) _callback(completed, _total, step, message);
      }
    };
  }

  SequenceRunner::SequenceRunner(dependency_container* container) :
    RunningChanged(_events),
    _driver(container->resolve<IDriverLink>()),
    _actionCompletedSubscription(_driver->ActionCompleted({ this, &SequenceRunner::OnActionCompleted })),
    _agentControl(container->resolve<AgentControl>()),
    _modeChangedSubscription(_agentControl->ModeChanged({ this, &SequenceRunner::OnModeChanged })),
    _captureStore(container->resolve<CaptureStore>()),
    _settings(container->resolve<SettingsStore>()),
    _actionLog(container->resolve<ActionLog>()),
    _visualization(container->resolve<VisualizationFeed>()),
    _input(container->resolve<IInputAvailability>())
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
    _actionLog->BeginSequence(state.Sequence, request.Description, uint32_t(request.Steps.size()));

    RunSequenceResponse response;
    {
      ProgressReporter reporter{ progress, uint32_t(request.Steps.size()), KeepAliveInterval };
      for (uint32_t index = 0; index < request.Steps.size(); index++)
      {
        auto& step = request.Steps[index];
        _actionLog->Start(step, index);
        reporter.StartStep(index, step->Name(), ExpectedDuration(*step, Remaining(state.Deadline)));
        auto visualId = _visualization->OnActionStarted(*step);
        auto result = RunStep(state, step, index);
        _visualization->OnActionFinished(visualId, *step, *result);
        _actionLog->Add(step, *result, exchange(state.CaptureFile, nullopt));

        if (result->Status != ActionStatus::Succeeded)
        {
          ReportFailure(response, state, index, *result);
          break;
        }

        response.Results.push_back(move(result));
        reporter.CompleteStep(index, step->Name());
      }
    }

    {
      lock_guard lock{ _mutex };
      _run = nullptr;
    }
    _actionLog->EndSequence(response);
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
    case ActionType::ProcessRun:
      return RunProcess(state, step);
    default:
      return IsInput(*step) ? ExecuteInput(state, step) : Execute(state, step);
    }
  }

  value_ptr<ActionResult> SequenceRunner::ExecuteInput(RunState& state, const value_ptr<Action>& step)
  {
    if (auto stopped = WaitWhileResuming(state)) return stopped;

    auto result = Execute(state, step);
    auto isRefused = [&] { return result->Status == ActionStatus::Failed && result->Error == ErrorCode::InputUnavailable; };

    //Refused as the session switched: again after the switch. The bridge may report the lock a moment after the
    //refusal, and Windows may report the session unlocked a moment before it takes input again. Refused at its first
    //event, the step did nothing; in the rare case the lock fell within it, it runs again from the start.
    for (uint32_t attempt = 0; isRefused() && attempt < _inputRetries; attempt++)
    {
      auto graceEnd = steady_clock::now() + _inputRefusalGrace;
      while (_input->Current() == InputAvailability::Unavailable && steady_clock::now() < graceEnd)
      {
        if (state.Stop.WaitFor(_inputPollInterval)) return ActionResult::Cancelled();
      }
      if (_input->Current() == InputAvailability::Unavailable) return result;

      if (auto stopped = WaitWhileResuming(state)) return stopped;
      if (state.Stop.WaitFor(_inputSettleTime)) return ActionResult::Cancelled();
      if (Remaining(state.Deadline) == 0ms) return ActionResult::Failed(ErrorCode::Timeout, _timeoutMessage);

      _logger.log(log_severity::information, "Running {} again, now that input is back.", step->Name());
      result = Execute(state, step);
    }
    return result;
  }

  value_ptr<ActionResult> SequenceRunner::WaitWhileResuming(RunState& state)
  {
    if (_input->Current() != InputAvailability::Resuming) return nullptr;

    _logger.log(log_severity::information, "Waiting for input: the user's session is being kept unlocked behind the lock screen.");
    auto limit = steady_clock::now() + _inputResumeLimit;
    while (_input->Current() == InputAvailability::Resuming)
    {
      //Never a wait of 0 ms, which waits forever
      auto remaining = Remaining(state.Deadline);
      if (remaining == 0ms) return ActionResult::Failed(ErrorCode::Timeout, _timeoutMessage);
      if (steady_clock::now() >= limit) break;  //the step then fails with InputUnavailable, and the agent hears why
      if (state.Stop.WaitFor(min(_inputPollInterval, remaining))) return ActionResult::Cancelled();
    }
    return nullptr;
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

  value_ptr<ActionResult> SequenceRunner::RunProcess(RunState& state, const value_ptr<Action>& step)
  {
    //The driver gets the user's default shell when the agent names none
    auto action = step;
    auto& run = static_cast<ProcessRunAction&>(*action);
    if (!run.Shell) run.Shell = _settings->Current().DefaultShell;
    return Execute(state, action);
  }

  value_ptr<ActionResult> SequenceRunner::StoreImage(RunState& state, value_ptr<ActionResult>&& result, CapturedImage& image, const optional<string>& path, const CaptureDestination& destination, string_view fileStem)
  {
    try
    {
      state.CaptureFile = _captureStore->Store(image, *result, path, destination, fileStem);
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
    if (_agentControl->Mode() == ControlMode::User) return "The user has taken control of the session; sequences fail until they give it back. Check-Session with waitForControl waits for that.";
    return "The user stopped the agent; sequences fail until they give control back. Check-Session with waitForControl waits for that.";
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
