#pragma once
#include "IDriverLink.h"

namespace SecondShell::Session
{
  //Accepts the driver on the driver pipe, applies the host's half of the session guard (Design.md, Session guard): the
  //driver must run in the child session the host expects, and only one driver is accepted. Then sends it actions.
  class DriverLink : public IDriverLink
  {
    inline static const Axodox::Infrastructure::logger _logger{ "DriverLink" };

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

  private:
    Axodox::Networking::named_pipe_messaging_server _server;
    Axodox::Infrastructure::event_subscription _clientConnectedSubscription;
    Axodox::Infrastructure::event_subscription _clientDisconnectedSubscription;

    mutable std::mutex _mutex;
    std::condition_variable _driverChanged;
    std::optional<uint32_t> _expectedSessionId;
    std::unordered_map<Axodox::Networking::messaging_channel*, std::shared_ptr<Connection>> _connections;
    std::shared_ptr<Connection> _driver;

    void OnClientConnected(Axodox::Networking::messaging_server* server, Axodox::Networking::messaging_channel* channel);
    void OnClientDisconnected(Axodox::Networking::messaging_server* server, Axodox::Networking::messaging_channel* channel);

    Protocol::DriverHelloResponse Accept(const std::shared_ptr<Connection>& connection);
    std::shared_ptr<Connection> Driver() const;
  };
}
