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

  //Destroyed in reverse order: the peer stops using the transport, the transport stops listening to the channel, then
  //the channel closes.
  struct HostClient::Connection
  {
    unique_ptr<named_pipe_messaging_channel> Channel;
    shared_ptr<PipeTransport> Transport;
    unique_ptr<JsonRpcPeer> Peer;
  };

  HostClient::HostClient(dependency_container* container) :
    HostClient(container, PipeNames::Mcp())
  { }

  HostClient::HostClient(dependency_container* container, string pipeName) :
    SequenceProgress(_events),
    _launcher(container->resolve<HostLauncher>()),
    _pipeName(move(pipeName))
  { }

  HostClient::~HostClient() = default;

  bool HostClient::IsRunning()
  {
    return Connect() != nullptr;
  }

  optional<JsonRpcError> HostClient::Start(const CancellationToken& cancellation)
  {
    if (Connect()) return nullopt;

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

    //Wait for its pipe
    auto deadline = steady_clock::now() + _startTimeout;
    while (!cancellation.WaitFor(_connectInterval))
    {
      if (Connect()) return nullopt;
      if (steady_clock::now() > deadline) return ProtocolErrors::Make(ErrorCode::Timeout, format("SecondShell did not start within {}.", duration_cast<seconds>(_startTimeout)));
    }

    return JsonRpcError{ JsonRpcErrorCode::RequestCancelled, "Starting SecondShell was cancelled." };
  }

  shared_ptr<HostClient::Connection> HostClient::Connect()
  {
    lock_guard lock{ _mutex };
    if (_connection && _connection->Transport->IsOpen()) return _connection;

    unique_ptr<named_pipe_messaging_channel> channel;
    try
    {
      channel = named_pipe_messaging_client::connect(_pipeName);
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::debug, "The host is not reachable: {}", exception.what());
      return nullptr;
    }

    //Subscribe before the channel opens, so no message is missed
    auto connection = make_shared<Connection>();
    connection->Channel = move(channel);
    connection->Transport = make_shared<PipeTransport>(connection->Channel.get());
    connection->Peer = make_unique<JsonRpcPeer>(connection->Transport);
    connection->Peer->AddNotificationHandler(this, &HostClient::OnSequenceProgress);
    connection->Peer->Start();
    connection->Channel->open();

    //Requests still using the previous connection finish with it, and the last one closes it
    _connection = connection;
    return connection;
  }

  JsonRpcPeer& HostClient::Peer(Connection& connection)
  {
    return *connection.Peer;
  }

  void HostClient::OnSequenceProgress(const SequenceProgressNotification& notification)
  {
    _events.raise(SequenceProgress, this, notification);
  }
}
