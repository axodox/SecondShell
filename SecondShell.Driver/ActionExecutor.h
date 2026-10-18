#pragma once
#include "CaptureActionHandler.h"
#include "ClipboardActionHandler.h"
#include "DriverProtocol.h"
#include "HostConnection.h"
#include "KeyboardActionHandler.h"
#include "MouseActionHandler.h"
#include "PenActionHandler.h"
#include "ProcessActionHandler.h"
#include "TouchActionHandler.h"
#include "WindowActionHandler.h"

namespace SecondShell::Driver
{
  //Answers the host's actions/execute and input/reset: routes each action to the handler of its kind, and runs async
  //actions in the background, reporting them with actions/completed.
  class ActionExecutor
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ActionExecutor" };

  public:
    //Adds its handlers to the host connection, between HostConnection::Open and Hello.
    explicit ActionExecutor(Axodox::Infrastructure::dependency_container* container);
    ~ActionExecutor();

  private:
    std::shared_ptr<HostConnection> _connection;
    std::shared_ptr<Input::InputStateTracker> _inputState;
    std::shared_ptr<MouseActionHandler> _mouse;
    std::shared_ptr<KeyboardActionHandler> _keyboard;
    std::shared_ptr<TouchActionHandler> _touch;
    std::shared_ptr<PenActionHandler> _pen;
    std::shared_ptr<CaptureActionHandler> _capture;
    std::shared_ptr<WindowActionHandler> _windows;
    std::shared_ptr<ProcessActionHandler> _processes;
    std::shared_ptr<ClipboardActionHandler> _clipboard;
    std::shared_ptr<Capture::ScreenCapturer> _capturer;

    std::mutex _mutex;
    std::unordered_map<uint64_t, std::shared_ptr<Rpc::CancellationToken>> _asyncActions;
    Axodox::Threading::lifetime_source _lifetime;

    Rpc::RpcResult<Protocol::ExecuteActionResponse> ExecuteAction(const Protocol::ExecuteActionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::ResetInputResponse> ResetInput(const Protocol::ResetInputRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::ScreenResponse> GetScreen(const Protocol::GetScreenRequest& request, const Rpc::CancellationToken& cancellation);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);
    void StartAsync(uint64_t actionId, Axodox::Infrastructure::value_ptr<Actions::Action> action);
    void CancelAsyncActions();
  };
}
