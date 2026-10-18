#include "pch.h"
#include "HostClient.h"
#include "PipeNames.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace SecondShell::Actions;
using namespace SecondShell::Environment;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Mcp
{
  const milliseconds _startTimeout{ 30000 };
  const milliseconds _connectInterval{ 250 };
  const milliseconds _firstAttemptTimeout{ 1000 };

  struct HostClient::Connection
  {
    shared_ptr<PipeTransport> Transport;
    unique_ptr<JsonRpcPeer> Peer;
  };

  HostClient::HostClient(dependency_container* container) :
    HostClient(container, PipeNames::Mcp())
  { }

  HostClient::HostClient(dependency_container* container, string pipeName) :
    SequenceProgress(_events),
    _launcher(container->resolve<HostLauncher>()),
    _client(pipeName),
    _connectedSubscription(_client.connected({ this, &HostClient::OnConnected }))
  { }

  HostClient::~HostClient()
  {
    //Waits for a connected handler in progress, and no new one replaces the connection while it is destroyed
    _connectedSubscription.reset();
  }

  bool HostClient::IsRunning()
  {
    return CurrentConnection() != nullptr;
  }

  optional<JsonRpcError> HostClient::Start(const CancellationToken& cancellation)
  {
    if (CurrentConnection()) return nullopt;

    try
    {
      _launcher->Launch();
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::error, "Could not start the host: {}", exception.what());
      return ProtocolErrors::Make(ErrorCode::SessionNotActive, format("Could not start SecondShell: {}", exception.what()));
    }
    catch (const winrt::hresult_error& error)
    {
      auto message = winrt::to_string(error.message());
      _logger.log(log_severity::error, "Could not start the host: {}", message);
      return ProtocolErrors::Make(ErrorCode::SessionNotActive, format("Could not start SecondShell: {}", message));
    }

    //The client connects once the host's pipe is there
    auto deadline = steady_clock::now() + _startTimeout;
    while (!cancellation.WaitFor(_connectInterval))
    {
      if (CurrentConnection()) return nullopt;
      if (steady_clock::now() > deadline) return ProtocolErrors::Make(ErrorCode::Timeout, format("SecondShell did not start within {}.", duration_cast<seconds>(_startTimeout)));
    }

    return JsonRpcError{ JsonRpcErrorCode::RequestCancelled, "Starting SecondShell was cancelled." };
  }

  shared_ptr<HostClient::Connection> HostClient::CurrentConnection()
  {
    //Not before first use, so nothing retries while SecondShell is not used. The first attempt is immediate, so a host
    //which is running is found here, and Start does not launch another one.
    call_once(_openOnce, [this] {
      _client.open();
      _hasConnected.wait(_firstAttemptTimeout);
      });

    lock_guard lock{ _mutex };
    if (_connection && _connection->Transport->IsOpen()) return _connection;
    return nullptr;
  }

  JsonRpcPeer& HostClient::Peer(Connection& connection)
  {
    return *connection.Peer;
  }

  void HostClient::OnConnected(messaging_client* /*client*/, messaging_channel* channel)
  {
    //Before the channel opens, so no message is missed
    auto connection = make_shared<Connection>();
    connection->Transport = make_shared<PipeTransport>(static_cast<named_pipe_messaging_channel*>(channel));
    connection->Peer = make_unique<JsonRpcPeer>(connection->Transport);
    connection->Peer->AddNotificationHandler(this, &HostClient::OnSequenceProgress);
    connection->Peer->Start();

    //Requests still using the previous connection finish with it; its transport is closed already
    {
      lock_guard lock{ _mutex };
      _connection = move(connection);
    }

    _hasConnected.set();
  }

  void HostClient::OnSequenceProgress(const SequenceProgressNotification& notification)
  {
    _events.raise(SequenceProgress, this, notification);
  }
}
