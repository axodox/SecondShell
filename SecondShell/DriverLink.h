#pragma once
#include "IDriverLink.h"

namespace SecondShell::Session
{
  //Accepts the driver on the driver pipe, applies the host's half of the session guard (Design.md, Session guard): the
  //driver must run in the child session the host expects, and only one driver is accepted. Then sends it actions.
  class DriverLink : public IDriverLink
  {
    inline static const Axodox::Infrastructure::logger _logger{ "DriverLink" };
    Axodox::Infrastructure::event_owner _events;

    class Connection;

  public:
    explicit DriverLink(Axodox::Infrastructure::dependency_container* container);
    DriverLink(Axodox::Infrastructure::dependency_container* container, std::string_view pipeName);
    ~DriverLink();

    //Opens the pipe; throws if another process has it.
    void Start();

    //The child session the driver must run in; empty: no driver is accepted, and the current one is dropped.
    void Expect(std::optional<uint32_t> sessionId);
    bool WaitForConnection(std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation);

    virtual bool IsConnected() const override;
    virtual std::optional<uint32_t> DriverProcessId() const override;
    virtual Rpc::RpcResult<Protocol::ExecuteActionResponse> Execute(const Protocol::ExecuteActionRequest& request, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation) override;
    virtual Rpc::RpcResult<Protocol::ResetInputResponse> ResetInput() override;

    //The screen size the driver sees; fails with SessionNotActive if no driver is connected.
    Rpc::RpcResult<Protocol::ScreenResponse> GetScreen();

    //The user's input in interactive mode, played at once; dropped if no driver is connected.
    void ForwardUserInput(const Protocol::ForwardUserInputNotification& input);

    //Clipboard sharing in interactive mode: the driver watches the session's clipboard while enabled, and takes the
    //user's; both fail with SessionNotActive if no driver is connected.
    Rpc::RpcResult<Protocol::EmptyResponse> SetClipboardSync(bool isEnabled);
    Rpc::RpcResult<Protocol::EmptyResponse> SetClipboard(const std::vector<Actions::ClipboardContent>& contents);

    //Shares the driver's frames with this process; fails with SessionNotActive if no driver is connected.
    Rpc::RpcResult<Protocol::SubscribeFramesResponse> SubscribeFrames();

    //The driver's frames are in a new ring; raised on the connection's thread.
    Axodox::Infrastructure::event_publisher<DriverLink*, const Protocol::SharedFrames&> FramesChanged;

    //The session's clipboard changed while syncing; raised on the connection's thread.
    Axodox::Infrastructure::event_publisher<DriverLink*, const std::vector<Actions::ClipboardContent>&> ClipboardChanged;

  private:
    Axodox::Networking::named_pipe_messaging_server _server;
    Axodox::Infrastructure::event_subscription _clientConnectedSubscription;
    Axodox::Infrastructure::event_subscription _clientDisconnectedSubscription;

    mutable std::mutex _mutex;
    std::condition_variable _driverChanged;
    std::optional<uint32_t> _expectedSessionId;
    std::unordered_map<Axodox::Networking::messaging_channel*, std::shared_ptr<Connection>> _connections;

    //Disconnected, destroyed later: not in the channel's disconnected event, whose next handler is their transport's
    std::vector<std::shared_ptr<Connection>> _closedConnections;
    std::shared_ptr<Connection> _driver;

    void OnClientConnected(Axodox::Networking::messaging_server* server, Axodox::Networking::messaging_channel* channel);
    void OnClientDisconnected(Axodox::Networking::messaging_server* server, Axodox::Networking::messaging_channel* channel);

    Protocol::DriverHelloResponse Accept(const std::shared_ptr<Connection>& connection);
    std::shared_ptr<Connection> Driver() const;
  };
}
