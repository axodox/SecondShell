#include "pch.h"
#include "McpRequestHandler.h"
#include "PipeNames.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace SecondShell::Environment;
using namespace std;

namespace SecondShell::Agent
{
  McpRequestHandler::McpRequestHandler(dependency_container* container) :
    _container(container->get_ref()),
    _server(PipeNames::Mcp()),
    _clientConnectedSubscription(_server.client_connected({ this, &McpRequestHandler::OnClientConnected })),
    _clientDisconnectedSubscription(_server.client_disconnected({ this, &McpRequestHandler::OnClientDisconnected }))
  { }

  McpRequestHandler::~McpRequestHandler()
  {
    //The connections go before the channels they use
    unordered_map<messaging_channel*, unique_ptr<McpConnection>> connections;
    {
      lock_guard lock{ _mutex };
      connections = move(_connections);
    }
  }

  void McpRequestHandler::Start()
  {
    _server.open();
  }

  void McpRequestHandler::OnClientConnected(messaging_server* /*server*/, messaging_channel* channel)
  {
    auto container = _container.try_lock();
    if (!container) return;

    auto connection = make_unique<McpConnection>(container.get(), static_cast<named_pipe_messaging_channel*>(channel));

    lock_guard lock{ _mutex };
    _connections[channel] = move(connection);
  }

  void McpRequestHandler::OnClientDisconnected(messaging_server* /*server*/, messaging_channel* channel)
  {
    //Destroyed outside the lock: its peer waits for its running requests
    unique_ptr<McpConnection> connection;
    {
      lock_guard lock{ _mutex };
      auto item = _connections.find(channel);
      if (item == _connections.end()) return;

      connection = move(item->second);
      _connections.erase(item);
    }
  }
}
