#include "pch.h"
#include "McpConnection.h"
#include "ProtocolErrors.h"
#include "WindowActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  const milliseconds _controlPollInterval{ 250 };
  const milliseconds _windowsTimeout{ 5000 };

  McpConnection::McpConnection(dependency_container* container, named_pipe_messaging_channel* channel) :
    _sessions(container->resolve<SessionManager>()),
    _sequences(container->resolve<SequenceRunner>()),
    _captures(container->resolve<CaptureStore>()),
    _lockScreen(container->resolve<LockScreenBridge>()),
    _agentControl(container->resolve<AgentControl>()),
    _eventLog(container->resolve<SessionEventLog>()),
    _driver(container->resolve<IDriverLink>()),
    _peer(make_shared<PipeTransport>(channel))
  {
    //The agent hears only about what happens from now on
    _eventPosition = _eventLog->End();

    _peer.AddHandler(this, &McpConnection::InitializeSession);
    _peer.AddHandler(this, &McpConnection::CheckSession);
    _peer.AddHandler(this, &McpConnection::ShutdownSession);
    _peer.AddHandler(this, &McpConnection::RunSequence);
    _peer.AddHandler(this, &McpConnection::SetDisplay);
    _peer.AddHandler(this, &McpConnection::GetDisplay);
    _peer.AddHandler(this, &McpConnection::ConfigureCapture);
    _peer.Start();
  }

  RpcResult<SessionResponse> McpConnection::InitializeSession(const InitializeSessionRequest& request, const CancellationToken& cancellation)
  {
    if (request.Capture) _captures->Configure(*request.Capture);
    if (auto error = _sessions->Start(request, cancellation)) return *error;

    SessionResponse response{ Info() };
    AddNotices(response.Events, response.NewWindows);
    return response;
  }

  RpcResult<SessionResponse> McpConnection::CheckSession(const CheckSessionRequest& request, const CancellationToken& cancellation)
  {
    //Until the agent may send input, unless there is no session to send it to
    if (request.WaitForControl)
    {
      auto deadline = steady_clock::now() + milliseconds(*request.WaitForControl);
      while (_driver->IsConnected() && !CanSendInput())
      {
        auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());
        if (remaining <= 0ms || cancellation.WaitFor(min(_controlPollInterval, remaining))) break;
      }
    }

    SessionResponse response{ Info() };
    AddNotices(response.Events, response.NewWindows);
    return response;
  }

  RpcResult<EmptyResponse> McpConnection::ShutdownSession(const ShutdownSessionRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    _sessions->Shutdown();
    return EmptyResponse{};
  }

  RpcResult<RunSequenceResponse> McpConnection::RunSequence(const RunSequenceRequest& request, const CancellationToken& cancellation)
  {
    ProgressCallback progress;
    if (request.ProgressToken)
    {
      progress = [&](uint32_t completed, uint32_t total, optional<double> stepProgress, string_view message) {
        _peer.Notify(SequenceProgressNotification{ request.ProgressToken, completed, total, string(message), stepProgress });
        };
    }

    auto response = _sequences->Run(request, progress, cancellation);
    if (response.Error == ErrorCode::InputUnavailable) response.Message = InputUnavailableMessage();

    //Nobody reads the answer of a cancelled request
    if (!cancellation.IsCancelled()) AddNotices(response.Events, response.NewWindows);
    return response;
  }

  RpcResult<DisplayResponse> McpConnection::SetDisplay(const SetDisplayRequest& request, const CancellationToken& cancellation)
  {
    if (auto error = _sessions->SetDisplay(request.Display, cancellation)) return *error;
    return DisplayResponse{ *_sessions->Info().Display };
  }

  RpcResult<DisplayResponse> McpConnection::GetDisplay(const GetDisplayRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    auto display = _sessions->Info().Display;
    if (!display) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "No session is running; start one with Initialize-Session.");

    return DisplayResponse{ *display };
  }

  RpcResult<CaptureSettingsResponse> McpConnection::ConfigureCapture(const ConfigureCaptureRequest& request, const CancellationToken& /*cancellation*/)
  {
    _captures->Configure(request.Capture);
    return CaptureSettingsResponse{ _captures->Settings() };
  }

  SessionInfo McpConnection::Info() const
  {
    auto info = _sessions->Info();
    auto status = _lockScreen->Status();
    info.IsUserPresent = status.IsUserPresent;
    info.Input = status.Input;
    info.Control = _agentControl->Mode();
    return info;
  }

  bool McpConnection::CanSendInput() const
  {
    return _agentControl->IsAgentAllowed() && _lockScreen->Status().Input == InputAvailability::Available;
  }

  void McpConnection::AddNotices(vector<SessionEvent>& events, vector<WindowInfo>& newWindows)
  {
    lock_guard lock{ _noticesMutex };
    events = _eventLog->ReadFrom(_eventPosition);

    //A new session has only new windows: the first look learns them
    if (!_driver->IsConnected())
    {
      _newWindows.Reset();
      return;
    }
    if (auto windows = Windows()) newWindows = _newWindows.Update(move(*windows));
  }

  optional<vector<WindowInfo>> McpConnection::Windows()
  {
    auto enumerate = make_value<WindowEnumerateAction>();
    enumerate->All = true;

    CancellationToken noCancellation;
    auto response = _driver->Execute(ExecuteActionRequest{ 0, value_ptr<Action>(move(enumerate)) }, _windowsTimeout, noCancellation);
    auto executed = get_if<ExecuteActionResponse>(&response);
    auto list = executed ? dynamic_cast<WindowListResult*>(executed->Result.get()) : nullptr;
    if (!list)
    {
      _logger.log(log_severity::warning, "Could not list the windows of the session for the agent.");
      return nullopt;
    }
    return move(list->Windows);
  }

  string McpConnection::InputUnavailableMessage() const
  {
    switch (_lockScreen->Status().Input)
    {
    case InputAvailability::Resuming:
      return "The user locked the PC, which stops all input to the session. SecondShell is keeping the user's session unlocked behind the lock screen, but input did not come back within 20 seconds; retry in a few seconds. Check-Session shows when input is Available.";
    case InputAvailability::Unavailable:
      return "The user's own Windows session is locked, which stops all input to the session until it is unlocked: seconds if the user is signing in at the PC, otherwise when they come back (Keep working while the screen is locked in SecondShell keeps it unlocked while they are away). Captures, windows, processes and the clipboard still work; Check-Session shows when input is Available.";
    default:
      return "Input was refused while the user's Windows session was switching, e.g. between the PC's screen and the connection that keeps it unlocked behind the lock screen. It is available again: retry.";
    }
  }
}
