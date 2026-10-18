#include "pch.h"
#include "DriverLink.h"
#include "JsonRpcPeer.h"
#include "PipeNames.h"
#include "PipeTransport.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace SecondShell::Actions;
using namespace SecondShell::Environment;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Session
{
  const milliseconds _resetTimeout{ 5000 };
  const milliseconds _screenTimeout{ 5000 };
  const milliseconds _cancellationPollInterval{ 100 };

  //One client of the driver pipe, accepted or not.
  class DriverLink::Connection : public enable_shared_from_this<Connection>
  {
  public:
    Connection(DriverLink* owner, named_pipe_messaging_channel* channel) :
      Transport(make_shared<PipeTransport>(channel)),
      Peer(Transport),
      _owner(owner)
    {
      Peer.AddHandler(this, &Connection::Hello);
      Peer.AddNotificationHandler(this, &Connection::OnActionCompleted);
      Peer.Start();
    }

    shared_ptr<PipeTransport> Transport;
    JsonRpcPeer Peer;

  private:
    DriverLink* _owner;

    RpcResult<DriverHelloResponse> Hello(const DriverHelloRequest& /*request*/, const CancellationToken& /*cancellation*/)
    {
      return _owner->Accept(shared_from_this());
    }

    void OnActionCompleted(const ActionCompletedNotification& notification)
    {
      if (_owner->Driver().get() == this) _owner->OnActionCompleted(notification);
    }
  };

  DriverLink::DriverLink(dependency_container* container) :
    DriverLink(container, PipeNames::Driver())
  { }

  DriverLink::DriverLink(dependency_container* /*container*/, string_view pipeName) :
    _server(pipeName),
    _clientConnectedSubscription(_server.client_connected({ this, &DriverLink::OnClientConnected })),
    _clientDisconnectedSubscription(_server.client_disconnected({ this, &DriverLink::OnClientDisconnected }))
  { }

  DriverLink::~DriverLink()
  {
    //The connections go before the channels they use
    unordered_map<messaging_channel*, shared_ptr<Connection>> connections;
    {
      lock_guard lock{ _mutex };
      _driver.reset();
      connections = move(_connections);
    }
  }

  void DriverLink::Start()
  {
    _server.open();
  }

  void DriverLink::Expect(optional<uint32_t> sessionId)
  {
    lock_guard lock{ _mutex };
    _expectedSessionId = sessionId;
    if (_driver && (!sessionId || _driver->Transport->PeerSessionId() != *sessionId)) _driver.reset();
    _driverChanged.notify_all();
  }

  bool DriverLink::WaitForConnection(milliseconds timeout, const CancellationToken& cancellation)
  {
    auto deadline = steady_clock::now() + timeout;
    unique_lock lock{ _mutex };
    while (!_driver)
    {
      if (cancellation.IsCancelled() || steady_clock::now() > deadline) return false;
      _driverChanged.wait_for(lock, _cancellationPollInterval);
    }
    return true;
  }

  bool DriverLink::IsConnected() const
  {
    auto driver = Driver();
    return driver && driver->Transport->IsOpen();
  }

  optional<uint32_t> DriverLink::DriverProcessId() const
  {
    auto driver = Driver();
    return driver ? optional<uint32_t>(driver->Transport->PeerProcessId()) : nullopt;
  }

  RpcResult<ExecuteActionResponse> DriverLink::Execute(const ExecuteActionRequest& request, milliseconds timeout, const CancellationToken& cancellation)
  {
    auto driver = Driver();
    if (!driver) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "The driver is not connected.");

    return driver->Peer.Request(request, timeout, cancellation);
  }

  RpcResult<ResetInputResponse> DriverLink::ResetInput()
  {
    auto driver = Driver();
    if (!driver) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "The driver is not connected.");

    CancellationToken noCancellation;
    return driver->Peer.Request(ResetInputRequest{}, _resetTimeout, noCancellation);
  }

  RpcResult<ScreenResponse> DriverLink::GetScreen()
  {
    auto driver = Driver();
    if (!driver) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "The driver is not connected.");

    CancellationToken noCancellation;
    return driver->Peer.Request(GetScreenRequest{}, _screenTimeout, noCancellation);
  }

  void DriverLink::OnClientConnected(messaging_server* /*server*/, messaging_channel* channel)
  {
    auto connection = make_shared<Connection>(this, static_cast<named_pipe_messaging_channel*>(channel));

    lock_guard lock{ _mutex };
    _connections[channel] = move(connection);
  }

  void DriverLink::OnClientDisconnected(messaging_server* /*server*/, messaging_channel* channel)
  {
    //Destroyed outside the lock: its peer waits for its handlers, which may be waiting for the lock
    shared_ptr<Connection> connection;
    {
      lock_guard lock{ _mutex };
      auto item = _connections.find(channel);
      if (item == _connections.end()) return;

      connection = move(item->second);
      _connections.erase(item);

      if (_driver == connection)
      {
        _driver.reset();
        _logger.log(log_severity::warning, "The driver disconnected.");
      }
    }
  }

  DriverHelloResponse DriverLink::Accept(const shared_ptr<Connection>& connection)
  {
    auto sessionId = connection->Transport->PeerSessionId();
    auto processId = connection->Transport->PeerProcessId();

    lock_guard lock{ _mutex };
    if (!_expectedSessionId)
    {
      _logger.log(log_severity::warning, "Refused the driver in session {} (process {}): no child session is waiting for one.", sessionId, processId);
      return { false, "No child session is waiting for a driver." };
    }

    if (sessionId != *_expectedSessionId)
    {
      _logger.log(log_severity::warning, "Refused the driver in session {} (process {}): the child session is {}.", sessionId, processId, *_expectedSessionId);
      return { false, format("The driver runs in session {}, not in the child session {}.", sessionId, *_expectedSessionId) };
    }

    if (_driver && _driver->Transport->IsOpen())
    {
      _logger.log(log_severity::warning, "Refused the driver in session {} (process {}): another driver is connected.", sessionId, processId);
      return { false, "Another driver is connected." };
    }

    _driver = connection;
    _driverChanged.notify_all();
    _logger.log(log_severity::information, "Accepted the driver in session {} (process {}).", sessionId, processId);
    return { true, "" };
  }

  shared_ptr<DriverLink::Connection> DriverLink::Driver() const
  {
    lock_guard lock{ _mutex };
    return _driver;
  }
}
