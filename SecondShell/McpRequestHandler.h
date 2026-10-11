#pragma once
#include "McpConnection.h"

namespace SecondShell::Agent
{
  //The host's MCP pipe: one McpConnection per connected MCP server. The pipe also makes the host single-instance:
  //the first host claims its name.
  class McpRequestHandler
  {
  public:
    explicit McpRequestHandler(Axodox::Infrastructure::dependency_container* container);
    ~McpRequestHandler();

    //Opens the pipe; throws if another host has it.
    void Start();

  private:
    Axodox::Infrastructure::dependency_container_ref _container;
    Axodox::Networking::named_pipe_messaging_server _server;
    Axodox::Infrastructure::event_subscription _clientConnectedSubscription;
    Axodox::Infrastructure::event_subscription _clientDisconnectedSubscription;

    std::mutex _mutex;
    std::unordered_map<Axodox::Networking::messaging_channel*, std::unique_ptr<McpConnection>> _connections;

    void OnClientConnected(Axodox::Networking::messaging_server* server, Axodox::Networking::messaging_channel* channel);
    void OnClientDisconnected(Axodox::Networking::messaging_server* server, Axodox::Networking::messaging_channel* channel);
  };
}
