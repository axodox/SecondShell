#include "pch.h"
#include "JsonRpcPeer.h"
#include "PipeTransport.h"
#include "TestPipeName.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Axodox::Networking;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  namespace
  {
    struct HelloResponse
    {
      static json_object_descriptor<HelloResponse> json_description;

      uint32_t PeerSessionId = 0;
      uint32_t PeerProcessId = 0;
    };

    struct HelloRequest
    {
      static json_object_descriptor<HelloRequest> json_description;
      using ResponseType = HelloResponse;
    };

    json_object_descriptor<HelloResponse> HelloResponse::json_description = describe_json_object<HelloResponse>({
      { &HelloResponse::PeerSessionId, "peerSessionId" },
      { &HelloResponse::PeerProcessId, "peerProcessId" }
      });
    json_object_descriptor<HelloRequest> HelloRequest::json_description = describe_json_object<HelloRequest>({ .name = "test/hello" }, {});

    //The server side of one connection: answers with what the pipe reports about the client, as the session guard will.
    class HelloService
    {
    public:
      explicit HelloService(named_pipe_messaging_channel* channel) :
        _transport(make_shared<PipeTransport>(channel)),
        _peer(_transport)
      {
        _peer.AddHandler(this, &HelloService::Hello);
        _peer.Start();
      }

    private:
      shared_ptr<PipeTransport> _transport;
      JsonRpcPeer _peer;

      RpcResult<HelloResponse> Hello(const HelloRequest& /*request*/, const CancellationToken& /*cancellation*/)
      {
        return HelloResponse{ _transport->PeerSessionId(), _transport->PeerProcessId() };
      }
    };
  }

  TEST_CLASS(PipeTransportTests)
  {
  public:
    TEST_METHOD(PeersTalkOverAPipe)
    {
      auto name = UniquePipeName("Transport");

      //Server: one service per connection, created before the channel opens
      named_pipe_messaging_server server{ name };
      mutex servicesMutex;
      vector<unique_ptr<HelloService>> services;
      auto clientConnectedSubscription = server.client_connected([&](messaging_server*, messaging_channel* channel) {
        lock_guard lock{ servicesMutex };
        services.push_back(make_unique<HelloService>(static_cast<named_pipe_messaging_channel*>(channel)));
        });
      server.open();

      //Client
      named_pipe_messaging_client client{ name };
      unique_ptr<JsonRpcPeer> clientPeer;
      Axodox::Threading::manual_reset_event clientReady;
      auto connectedSubscription = client.connected([&](messaging_client*, messaging_channel* channel) {
        clientPeer = make_unique<JsonRpcPeer>(make_shared<PipeTransport>(static_cast<named_pipe_messaging_channel*>(channel)));
        clientPeer->Start();
        clientReady.set();
        });
      client.open();
      Assert::IsTrue(clientReady.wait(5s), L"the client should connect");

      //Request
      CancellationToken noCancellation;
      auto result = clientPeer->Request(HelloRequest{}, 5s, noCancellation);
      if (auto error = get_if<JsonRpcError>(&result)) Assert::Fail(to_wstring(error->Message).c_str());

      DWORD sessionId = 0;
      ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

      auto& response = get<HelloResponse>(result);
      Assert::AreEqual(uint32_t(sessionId), response.PeerSessionId);
      Assert::AreEqual(uint32_t(GetCurrentProcessId()), response.PeerProcessId);

      //The peers go before the connections they use
      clientPeer.reset();
      lock_guard lock{ servicesMutex };
      services.clear();
    }
  };
}
