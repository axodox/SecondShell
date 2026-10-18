#include "pch.h"
#include "HostConnection.h"
#include "DriverProtocol.h"
#include "PipeNames.h"
#include "ProductInfo.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace SecondShell::Environment;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  namespace
  {
    uint32_t CurrentSessionId()
    {
      DWORD sessionId = 0;
      winrt::check_bool(ProcessIdToSessionId(GetCurrentProcessId(), &sessionId));
      return sessionId;
    }
  }

  HostConnection::HostConnection(dependency_container* /*container*/) :
    _client(PipeNames::Driver()),
    _connectedSubscription(_client.connected({ this, &HostConnection::OnConnected }))
  { }

  HostConnection::~HostConnection()
  {
    //Waits for a connected handler in progress, e.g. a host appearing after Open timed out, and prevents new ones
    _connectedSubscription.reset();
  }

  bool HostConnection::Open(milliseconds timeout)
  {
    //The client retries until the host is there, e.g. while it is busy accepting another connection
    _client.open();
    if (!_connected.wait(timeout))
    {
      _logger.log(log_severity::warning, "No host within {}.", duration_cast<seconds>(timeout));
      return false;
    }

    return _peer != nullptr;
  }

  void HostConnection::OnConnected(messaging_client* client, messaging_channel* channel)
  {
    //A host in this session means this is not its child session, e.g. a leftover startup entry ran at a normal logon;
    //closing the client before the channel opens leaves it unopened, and stops the reconnects
    auto pipe = static_cast<named_pipe_messaging_channel*>(channel);
    auto sessionId = CurrentSessionId();
    if (pipe->remote_peer().session_id == sessionId)
    {
      _logger.log(log_severity::warning, "The host is in this session ({}), so this is not its child session.", sessionId);
      client->close();
    }
    else
    {
      //Without reconnecting after the host is gone, the transport and the peer are never replaced while in use
      _transport = make_shared<PipeTransport>(pipe);
      _peer = make_unique<JsonRpcPeer>(_transport);
      _closedSubscription = _transport->Closed([this](IMessageTransport*) {
        _client.close();
        _closed.set();
        });
    }

    _connected.set();
  }

  JsonRpcPeer& HostConnection::Peer()
  {
    return *_peer;
  }

  bool HostConnection::Hello(milliseconds timeout)
  {
    _peer->Start();

    CancellationToken noCancellation;
    auto result = _peer->Request(DriverHelloRequest{ ProductInfo::Version() }, timeout, noCancellation);
    if (auto error = get_if<JsonRpcError>(&result))
    {
      _logger.log(log_severity::warning, "The host did not answer: {}", error->Message);
      return false;
    }

    auto& response = get<DriverHelloResponse>(result);
    if (!response.IsAccepted)
    {
      _logger.log(log_severity::warning, "The host refused this driver: {}", response.Reason);
      return false;
    }

    _isAccepted = true;
    _logger.log(log_severity::information, "Accepted by the host (process {}).", _transport->PeerProcessId());
    return true;
  }

  uint32_t HostConnection::HostProcessId() const
  {
    return _transport->PeerProcessId();
  }

  bool HostConnection::IsAccepted() const
  {
    return _isAccepted;
  }

  void HostConnection::WaitUntilClosed()
  {
    _closed.wait();
  }
}
