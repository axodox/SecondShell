#pragma once
#include "MemoryTransport.h"

namespace SecondShell::Tests
{
  //The client side of an MCP server under test, over an in-memory transport: sends JSON-RPC text, and receives the
  //server's messages in the order they were sent.
  class McpTestClient
  {
  public:
    McpTestClient();

    //The transport for the server.
    std::shared_ptr<Rpc::IMessageTransport> ServerTransport() const;

    void Send(std::string_view message);

    //The server's next message; fails the test if none arrives in time.
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Receive();

    //Sends a request and returns the result of its response, which must not be an error.
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Call(std::string_view method, std::string_view params);

    //Sends tools/call and returns the result.
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> CallTool(std::string_view name, std::string_view arguments);

    //Whether the server sent nothing within the given time.
    bool IsSilent(std::chrono::milliseconds duration);

  private:
    std::shared_ptr<MemoryTransport> _client;
    std::shared_ptr<MemoryTransport> _server;
    Axodox::Threading::blocking_collection<std::string> _messages;
    Axodox::Infrastructure::event_subscription _messageReceivedSubscription;
    int _nextId = 1;
  };
}
