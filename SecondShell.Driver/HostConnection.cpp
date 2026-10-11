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
  const milliseconds _connectInterval{ 500 };

  HostConnection::HostConnection(dependency_container* /*container*/)
  { }

  HostConnection::~HostConnection() = default;

  bool HostConnection::Open(milliseconds timeout)
  {
    //The host may be busy accepting another connection, so try for a while
    auto deadline = steady_clock::now() + timeout;
    while (!_channel)
    {
      try
      {
        _channel = named_pipe_messaging_client::connect(PipeNames::Driver());
      }
      catch (const exception& exception)
      {
        if (steady_clock::now() > deadline)
        {
          _logger.log(log_severity::warning, "No host: {}", exception.what());
          return false;
        }
        this_thread::sleep_for(_connectInterval);
      }
    }

    //A host in this session means this is not its child session, e.g. a leftover startup entry ran at a normal logon
    DWORD sessionId = 0;
    winrt::check_bool(ProcessIdToSessionId(GetCurrentProcessId(), &sessionId));
    if (_channel->peer_session_id() == sessionId)
    {
      _logger.log(log_severity::warning, "The host is in this session ({}), so this is not its child session.", sessionId);
      return false;
    }

    _transport = make_shared<PipeTransport>(_channel.get());
    _peer = make_unique<JsonRpcPeer>(_transport);
    _closedSubscription = _transport->Closed([this](IMessageTransport*) { _closed.set(); });
    return true;
  }

  JsonRpcPeer& HostConnection::Peer()
  {
    return *_peer;
  }

  bool HostConnection::Hello(milliseconds timeout)
  {
    _peer->Start();
    _channel->open();

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
    _logger.log(log_severity::information, "Accepted by the host (process {}).", _channel->peer_process_id());
    return true;
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
