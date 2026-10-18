#pragma once
#include "ChildSessionApi.h"
#include "CredentialProvider.h"
#include "DriverLauncher.h"
#include "DriverLink.h"
#include "HostProtocol.h"
#include "RdpClient.h"

namespace SecondShell::Session
{
  //The session lifecycle: start (reset a left-over session, sign in, start the driver), shutdown. The only class that
  //sequences these steps.
  class SessionManager
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SessionManager" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit SessionManager(Axodox::Infrastructure::dependency_container* container);
    ~SessionManager();

    Protocol::SessionInfo Info() const;

    //Returns once the driver is connected, or with the reason it failed. Does nothing if the session is ready.
    std::optional<Rpc::JsonRpcError> Start(const Protocol::InitializeSessionRequest& request, const Rpc::CancellationToken& cancellation);

    //Always logs the session off.
    void Shutdown();

    //Changes the display of the running session, keeping what the change leaves out; returns once the driver sees the
    //new size, so captures show it.
    std::optional<Rpc::JsonRpcError> SetDisplay(const Protocol::DisplayChange& change, const Rpc::CancellationToken& cancellation);

    Axodox::Infrastructure::event_publisher<SessionManager*, Protocol::SessionState> StateChanged;

  private:
    std::shared_ptr<ChildSessionApi> _childSessions;
    std::shared_ptr<CredentialProvider> _credentials;
    std::shared_ptr<DriverLauncher> _driverLauncher;
    std::shared_ptr<DriverLink> _driverLink;
    std::shared_ptr<RdpClient> _rdpClient;
    Axodox::Infrastructure::event_subscription _loginCompletedSubscription;
    Axodox::Infrastructure::event_subscription _disconnectedSubscription;

    std::mutex _startMutex;  //one start or shutdown at a time
    mutable std::mutex _mutex;
    Protocol::SessionState _state = Protocol::SessionState::Stopped;
    std::optional<uint32_t> _sessionId;
    std::optional<Protocol::DisplaySettings> _display;
    bool _hasConnected = false;  //a child session may exist because of this host

    Axodox::Threading::manual_reset_event _loginCompleted;
    Axodox::Threading::manual_reset_event _disconnected;
    std::atomic<long> _disconnectReason = 0;

    std::optional<Rpc::JsonRpcError> ResetExistingSession(const std::optional<Protocol::ExistingSessionPolicy>& policy);
    std::optional<Rpc::JsonRpcError> SignIn(const Protocol::DisplaySettings& display, std::chrono::steady_clock::time_point deadline, const Rpc::CancellationToken& cancellation);
    std::optional<Rpc::JsonRpcError> ConnectAndWaitForSignIn(const Protocol::DisplaySettings& display, std::chrono::steady_clock::time_point deadline, const Rpc::CancellationToken& cancellation);
    std::optional<Rpc::JsonRpcError> StartDriver(std::chrono::steady_clock::time_point deadline, const Rpc::CancellationToken& cancellation);
    void CleanUp();
    void SetState(Protocol::SessionState state);

    void OnLoginCompleted(RdpClient* sender);
    void OnDisconnected(RdpClient* sender, long reason);
  };
}
