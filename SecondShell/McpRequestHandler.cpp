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
    vector<unique_ptr<McpConnection>> closed;
    {
      lock_guard lock{ _mutex };
      connections = move(_connections);
      closed = move(_closedConnections);
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

    //The connections closed before go now, outside the lock: their peers wait for their running requests
    vector<unique_ptr<McpConnection>> closed;
    lock_guard lock{ _mutex };
    _connections[channel] = move(connection);
    closed = exchange(_closedConnections, {});
  }

  void McpRequestHandler::OnClientDisconnected(messaging_server* /*server*/, messaging_channel* channel)
  {
    //Kept, not destroyed: this runs in the channel's disconnected event, which calls the connection's transport next
    //(Axodox calls a handler removed during the event all the same)
    lock_guard lock{ _mutex };
    auto item = _connections.find(channel);
    if (item == _connections.end()) return;

    _closedConnections.push_back(move(item->second));
    _connections.erase(item);
  }
}
