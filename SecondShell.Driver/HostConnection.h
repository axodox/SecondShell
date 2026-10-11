#pragma once
#include "JsonRpcPeer.h"
#include "PipeTransport.h"

namespace SecondShell::Driver
{
  //The driver's connection to the host, and the driver's half of the session guard (Design.md, Session guard): the host
  //must be in another session, and must accept this driver's session with driver/hello. Nothing touches input or the
  //screen before Hello returns true.
  class HostConnection
  {
    inline static const Axodox::Infrastructure::logger _logger{ "HostConnection" };

  public:
    explicit HostConnection(Axodox::Infrastructure::dependency_container* container);
    ~HostConnection();

    //Connects to the host's driver pipe; false if there is none, or if it is in this session (not a child session).
    bool Open(std::chrono::milliseconds timeout);

    //For the handlers, which are added between Open and Hello.
    Rpc::JsonRpcPeer& Peer();

    //Starts the peer and asks the host to accept this driver; false if it does not.
    bool Hello(std::chrono::milliseconds timeout);
    bool IsAccepted() const;

    void WaitUntilClosed();

  private:
    std::unique_ptr<Axodox::Networking::named_pipe_messaging_channel> _channel;
    std::shared_ptr<Rpc::PipeTransport> _transport;
    std::unique_ptr<Rpc::JsonRpcPeer> _peer;
    Axodox::Infrastructure::event_subscription _closedSubscription;
    Axodox::Threading::manual_reset_event _closed;
    std::atomic_bool _isAccepted = false;
  };
}
