#include "pch.h"
#include "McpTestClient.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  McpTestClient::McpTestClient()
  {
    tie(_client, _server) = MemoryTransport::CreatePair();
    _messageReceivedSubscription = _client->MessageReceived([this](IMessageTransport*, string_view message) { _messages.add(string(message)); });
  }

  shared_ptr<IMessageTransport> McpTestClient::ServerTransport() const
  {
    return _server;
  }

  void McpTestClient::Send(string_view message)
  {
    _client->Send(message);
  }

  value_ptr<json_value> McpTestClient::Receive()
  {
    string message;
    Assert::IsTrue(_messages.try_get(message, 5s), L"the server should send a message");

    string_view text = message;
    auto json = json_value::from_string(text);
    Assert::IsTrue(bool(json), L"the server should send valid JSON");
    return json;
  }

  value_ptr<json_value> McpTestClient::Call(string_view method, string_view params)
  {
    auto id = _nextId++;
    Send(format(R"({{"jsonrpc": "2.0", "id": {}, "method": "{}", "params": {}}})", id, method, params));

    auto response = Receive();
    auto& object = response->as<json_object>();
    Assert::AreEqual(double(id), object.at("id")->as<json_number>().value, L"the response should answer the request");
    Assert::IsFalse(object.value.contains("error"), to_wstring(response->to_string()).c_str());
    return object.at("result");
  }

  value_ptr<json_value> McpTestClient::CallTool(string_view name, string_view arguments)
  {
    return Call("tools/call", format(R"({{"name": "{}", "arguments": {}}})", name, arguments));
  }

  bool McpTestClient::IsSilent(chrono::milliseconds duration)
  {
    string message;
    return !_messages.try_get(message, duration);
  }
}
