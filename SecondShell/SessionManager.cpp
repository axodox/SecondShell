#include "pch.h"
#include "SessionManager.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Session
{
  const milliseconds _logOffTimeout{ 15000 };
  const milliseconds _pollInterval{ 100 };
  const milliseconds _displayTimeout{ 10000 };

  namespace
  {
    milliseconds Remaining(steady_clock::time_point deadline)
    {
      return max(duration_cast<milliseconds>(deadline - steady_clock::now()), milliseconds(0));
    }
  }

  SessionManager::SessionManager(dependency_container* container) :
    StateChanged(_events),
    _childSessions(container->resolve<ChildSessionApi>()),
    _credentials(container->resolve<CredentialProvider>()),
    _driverLauncher(container->resolve<DriverLauncher>()),
    _driverLink(container->resolve<DriverLink>()),
    _rdpClient(container->resolve<RdpClient>()),
    _loginCompletedSubscription(_rdpClient->LoginCompleted({ this, &SessionManager::OnLoginCompleted })),
    _disconnectedSubscription(_rdpClient->Disconnected({ this, &SessionManager::OnDisconnected }))
  { }

  SessionManager::~SessionManager()
  {
    Shutdown();
  }

  SessionInfo SessionManager::Info() const
  {
    lock_guard lock{ _mutex };
    return {
      .State = _state,
      .SessionId = _sessionId,
      .Display = _display,
      .IsDriverConnected = _driverLink->IsConnected(),
      .DriverProcessId = _driverLink->DriverProcessId()
    };
  }

  optional<JsonRpcError> SessionManager::Start(const InitializeSessionRequest& request, const CancellationToken& cancellation)
  {
    lock_guard startLock{ _startMutex };
    if (Info().State == SessionState::Ready && _driverLink->IsConnected()) return nullopt;

    if (!_childSessions->IsEnabled()) return ProtocolErrors::Make(ErrorCode::SetupRequired, "Child sessions are not enabled on this machine; run SecondShell.exe --setup once, as an administrator.");

    auto deadline = steady_clock::now() + (request.Timeout ? milliseconds(*request.Timeout) : InitializeSessionRequest::DefaultTimeout);
    if (auto error = ResetExistingSession(request.ExistingSession)) return error;

    auto error = SignIn(request.Display.value_or(DisplaySettings{}), deadline, cancellation);
    if (!error) error = StartDriver(deadline, cancellation);

    if (error)
    {
      _logger.log(log_severity::error, "The session could not start: {}", error->Message);
      CleanUp();
      SetState(SessionState::Failed);
      return error;
    }

    SetState(SessionState::Ready);
    return nullopt;
  }

  void SessionManager::Shutdown()
  {
    lock_guard startLock{ _startMutex };
    if (Info().State == SessionState::Stopped) return;

    SetState(SessionState::Stopping);
    CleanUp();
    SetState(SessionState::Stopped);
  }

  optional<JsonRpcError> SessionManager::SetDisplay(const DisplayChange& change, const CancellationToken& cancellation)
  {
    lock_guard startLock{ _startMutex };
    auto info = Info();
    if (info.State != SessionState::Ready || !info.Display) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "No session is running; start one with Initialize-Session.");

    auto display = change.ApplyTo(*info.Display);

    try
    {
      _rdpClient->UpdateDisplay(display);
    }
    catch (const exception& exception)
    {
      return ProtocolErrors::Make(ErrorCode::ActionFailed, format("The session did not take the display settings: {}", exception.what()));
    }
    catch (const winrt::hresult_error& error)
    {
      return ProtocolErrors::Make(ErrorCode::ActionFailed, format("The session did not take the display settings: {}", winrt::to_string(error.message())));
    }

    //A rotated screen may report its sides either way
    auto isApplied = [&] {
      auto screen = _driverLink->GetScreen();
      auto size = get_if<ScreenResponse>(&screen);
      return size && minmax(size->Width, size->Height) == minmax(display.Width, display.Height);
      };

    auto deadline = steady_clock::now() + _displayTimeout;
    while (!isApplied())
    {
      if (steady_clock::now() > deadline) return ProtocolErrors::Make(ErrorCode::Timeout, format("The session did not change to {}x{} within {}.", display.Width, display.Height, duration_cast<seconds>(_displayTimeout)));
      if (cancellation.WaitFor(_pollInterval)) return JsonRpcError{ JsonRpcErrorCode::RequestCancelled, "Changing the display was cancelled." };
    }

    lock_guard lock{ _mutex };
    _display = display;
    return nullopt;
  }

  optional<JsonRpcError> SessionManager::ResetExistingSession(const optional<ExistingSessionPolicy>& policy)
  {
    auto existing = _childSessions->ChildSessionId();
    if (!existing) return nullopt;

    //It may belong to another tool, so it is logged off only when asked to. The driver of a session from before exited
    //with its host, so resuming one needs a way to restart the driver first.
    if (!policy) return ProtocolErrors::Make(ErrorCode::ActionFailed, format("A child session ({}) already exists, e.g. from before a restart; start with existingSession Reset to log it off, closing its applications.", *existing));
    if (policy == ExistingSessionPolicy::Resume) return ProtocolErrors::Make(ErrorCode::ActionFailed, "Resuming a session from before is not supported yet; use existingSession Reset to log it off.");

    _logger.log(log_severity::information, "Logging off the session from before ({}).", *existing);
    if (!_childSessions->LogOff(*existing, _logOffTimeout)) return ProtocolErrors::Make(ErrorCode::Timeout, format("The session from before ({}) did not log off.", *existing));
    return nullopt;
  }

  optional<JsonRpcError> SessionManager::SignIn(const DisplaySettings& display, steady_clock::time_point deadline, const CancellationToken& cancellation)
  {
    _loginCompleted.reset();
    _disconnected.reset();
    _disconnectReason = 0;
    _driverLauncher->Arm();

    SetState(SessionState::Connecting);
    {
      lock_guard lock{ _mutex };
      _hasConnected = true;
    }

    if (auto error = ConnectAndWaitForSignIn(display, deadline, cancellation))
    {
      _credentials->OnSignInFailed(_disconnectReason);
      return error;
    }

    _credentials->OnSignInSucceeded();

    auto sessionId = _childSessions->ChildSessionId();
    if (!sessionId) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "Signed in, but there is no child session.");

    lock_guard lock{ _mutex };
    _sessionId = sessionId;
    _display = display;
    return nullopt;
  }

  optional<JsonRpcError> SessionManager::ConnectAndWaitForSignIn(const DisplaySettings& display, steady_clock::time_point deadline, const CancellationToken& cancellation)
  {
    try
    {
      _rdpClient->Connect(display);
    }
    catch (const exception& exception)
    {
      return ProtocolErrors::Make(ErrorCode::ActionFailed, exception.what());
    }
    catch (const winrt::hresult_error& error)
    {
      return ProtocolErrors::Make(ErrorCode::ActionFailed, format("Connecting to the child session failed: {}", winrt::to_string(error.message())));
    }

    SetState(SessionState::SigningIn);
    while (!_loginCompleted.wait(_pollInterval))
    {
      if (_disconnected) return ProtocolErrors::Make(ErrorCode::SessionNotActive, format("Signing in to the child session failed (disconnect reason {}).", _disconnectReason.load()));
      if (cancellation.IsCancelled()) return JsonRpcError{ JsonRpcErrorCode::RequestCancelled, "Starting the session was cancelled." };
      if (steady_clock::now() > deadline) return ProtocolErrors::Make(ErrorCode::Timeout, "Signing in to the child session did not complete in time.");
    }

    return nullopt;
  }

  optional<JsonRpcError> SessionManager::StartDriver(steady_clock::time_point deadline, const CancellationToken& cancellation)
  {
    SetState(SessionState::StartingDriver);
    _driverLink->Expect(Info().SessionId);

    if (!_driverLink->WaitForConnection(Remaining(deadline), cancellation))
    {
      if (cancellation.IsCancelled()) return JsonRpcError{ JsonRpcErrorCode::RequestCancelled, "Starting the session was cancelled." };
      return ProtocolErrors::Make(ErrorCode::Timeout, "The driver did not connect from the child session in time; see driver.log in the SecondShell log folder.");
    }

    //Windows deleted the entry when it ran it
    _driverLauncher->Disarm();
    return nullopt;
  }

  void SessionManager::CleanUp()
  {
    _driverLink->Expect(nullopt);
    _driverLauncher->Disarm();
    _rdpClient->Disconnect();

    bool hasConnected;
    {
      lock_guard lock{ _mutex };
      hasConnected = exchange(_hasConnected, false);
      _sessionId.reset();
      _display.reset();
    }

    //Disconnecting does not end a child session reliably (Findings.md 2.3)
    auto sessionId = _childSessions->ChildSessionId();
    if (hasConnected && sessionId && !_childSessions->LogOff(*sessionId, _logOffTimeout))
    {
      _logger.log(log_severity::error, "The child session {} did not log off.", *sessionId);
    }
  }

  void SessionManager::SetState(SessionState state)
  {
    {
      lock_guard lock{ _mutex };
      if (_state == state) return;
      _state = state;
    }

    _logger.log(log_severity::information, "State: {}.", to_string(state));
    _events.raise(StateChanged, this, state);
  }

  void SessionManager::OnLoginCompleted(RdpClient* /*sender*/)
  {
    _loginCompleted.set();
  }

  void SessionManager::OnDisconnected(RdpClient* /*sender*/, long reason)
  {
    _disconnectReason = reason;
    _disconnected.set();

    //The session was lost after it had started
    if (Info().State == SessionState::Ready)
    {
      _driverLink->Expect(nullopt);
      SetState(SessionState::Failed);
    }
  }
}
