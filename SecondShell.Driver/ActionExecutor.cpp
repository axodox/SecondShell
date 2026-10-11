#include "pch.h"
#include "ActionExecutor.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Input;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Driver
{
  ActionExecutor::ActionExecutor(dependency_container* container) :
    _connection(container->resolve<HostConnection>()),
    _inputState(container->resolve<InputStateTracker>()),
    _mouse(container->resolve<MouseActionHandler>()),
    _keyboard(container->resolve<KeyboardActionHandler>()),
    _capture(container->resolve<CaptureActionHandler>())
  {
    _connection->Peer().AddHandler(this, &ActionExecutor::ExecuteAction);
    _connection->Peer().AddHandler(this, &ActionExecutor::ResetInput);
  }

  ActionExecutor::~ActionExecutor()
  {
    //_lifetime then waits for them to return
    CancelAsyncActions();
  }

  RpcResult<ExecuteActionResponse> ActionExecutor::ExecuteAction(const ExecuteActionRequest& request, const CancellationToken& cancellation)
  {
    //The session guard: nothing runs before the host has accepted this driver
    if (!_connection->IsAccepted()) return JsonRpcError{ JsonRpcErrorCode::InvalidRequest, "The host has not accepted this driver." };
    if (!request.Action) return JsonRpcError{ JsonRpcErrorCode::InvalidParams, "The action is missing." };

    //Keeps this alive until the handler returns, whatever the order the services are destroyed in
    auto guard = _lifetime.try_guard();
    if (!guard) return JsonRpcError{ JsonRpcErrorCode::ConnectionClosed, "The driver is shutting down." };

    if (request.Action->IsAsync())
    {
      StartAsync(request.ActionId, request.Action);
      return ExecuteActionResponse{ ActionResult::Succeeded() };
    }

    return ExecuteActionResponse{ Execute(*request.Action, cancellation) };
  }

  RpcResult<ResetInputResponse> ActionExecutor::ResetInput(const ResetInputRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    if (!_connection->IsAccepted()) return JsonRpcError{ JsonRpcErrorCode::InvalidRequest, "The host has not accepted this driver." };

    auto guard = _lifetime.try_guard();
    if (!guard) return JsonRpcError{ JsonRpcErrorCode::ConnectionClosed, "The driver is shutting down." };

    CancelAsyncActions();
    return ResetInputResponse{ _inputState->ReleaseAll() };
  }

  value_ptr<ActionResult> ActionExecutor::Execute(const Action& action, const CancellationToken& cancellation)
  {
    try
    {
      switch (action.Kind())
      {
      case ActionKind::Mouse:
        return _mouse->Execute(action, cancellation);
      case ActionKind::Keyboard:
        return _keyboard->Execute(action, cancellation);
      case ActionKind::Capture:
        return _capture->Execute(action, cancellation);
      default:
        return ActionResult::Failed(ErrorCode::ActionFailed, format("{} is not available yet.", action.Name()));
      }
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::error, "{} failed: {}", action.Name(), exception.what());
      return ActionResult::Failed(ErrorCode::ActionFailed, format("{} failed: {}", action.Name(), exception.what()));
    }
  }

  void ActionExecutor::StartAsync(uint64_t actionId, value_ptr<Action> action)
  {
    auto guard = _lifetime.try_guard();
    if (!guard) return;

    auto cancellation = make_shared<CancellationToken>();
    {
      lock_guard lock{ _mutex };
      _asyncActions[actionId] = cancellation;
    }

    thread([this, guard = move(guard), actionId, action = move(action), cancellation] {
      auto result = Execute(*action, *cancellation);
      {
        lock_guard lock{ _mutex };
        _asyncActions.erase(actionId);
      }

      _connection->Peer().Notify(ActionCompletedNotification{ actionId, move(result) });
      }).detach();
  }

  void ActionExecutor::CancelAsyncActions()
  {
    vector<shared_ptr<CancellationToken>> running;
    {
      lock_guard lock{ _mutex };
      for (auto& [actionId, cancellation] : _asyncActions) running.push_back(cancellation);
    }

    for (auto& cancellation : running) cancellation->Cancel();
  }
}
