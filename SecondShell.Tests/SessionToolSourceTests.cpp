#include "pch.h"
#include "CaptureActions.h"
#include "JsonSnapshot.h"
#include "McpServer.h"
#include "McpTestClient.h"
#include "SessionToolSource.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Axodox::Networking;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Mcp;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
#pragma region Fake host
  namespace
  {
    //What the fake host received.
    struct HostRequests
    {
      mutex Mutex;
      optional<InitializeSessionRequest> Initialize;
      optional<RunSequenceRequest> Sequence;
    };

    //One connection of the fake host: answers the host's methods like a host with a ready session would.
    class FakeHostConnection
    {
    public:
      FakeHostConnection(named_pipe_messaging_channel* channel, HostRequests& requests) :
        _requests(requests),
        _peer(make_shared<PipeTransport>(channel))
      {
        _peer.AddHandler(this, &FakeHostConnection::InitializeSession);
        _peer.AddHandler(this, &FakeHostConnection::CheckSession);
        _peer.AddHandler(this, &FakeHostConnection::RunSequence);
        _peer.AddHandler(this, &FakeHostConnection::ConfigureCapture);
        _peer.Start();
      }

    private:
      HostRequests& _requests;
      JsonRpcPeer _peer;

      static SessionResponse ReadySession()
      {
        return { .Session = { .State = SessionState::Ready, .SessionId = 3, .IsDriverConnected = true, .DriverProcessId = 1234 } };
      }

      RpcResult<SessionResponse> InitializeSession(const InitializeSessionRequest& request, const CancellationToken& /*cancellation*/)
      {
        lock_guard lock{ _requests.Mutex };
        _requests.Initialize = request;
        return ReadySession();
      }

      RpcResult<SessionResponse> CheckSession(const CheckSessionRequest& /*request*/, const CancellationToken& /*cancellation*/)
      {
        return ReadySession();
      }

      RpcResult<RunSequenceResponse> RunSequence(const RunSequenceRequest& request, const CancellationToken& /*cancellation*/)
      {
        {
          lock_guard lock{ _requests.Mutex };
          _requests.Sequence = request;
        }

        if (request.ProgressToken) _peer.Notify(SequenceProgressNotification{ .ProgressToken = request.ProgressToken, .CompletedSteps = 1, .TotalSteps = 1, .Message = "Capture.Screen" });

        auto capture = make_value<CaptureResult>();
        capture->Tag = "0";
        capture->Image = { .Width = 4, .Height = 2, .Scale = 0.5, .Format = ImageFormat::Png, .Data = { 1, 2, 3 } };

        RunSequenceResponse response;
        response.Results.push_back(move(capture));
        return response;
      }

      RpcResult<CaptureSettingsResponse> ConfigureCapture(const ConfigureCaptureRequest& /*request*/, const CancellationToken& /*cancellation*/)
      {
        return ProtocolErrors::Make(ErrorCode::AgentInputDisabled, "The user has taken control.");
      }
    };

    //A host on a pipe of the test's own, one FakeHostConnection per client.
    class FakeHost
    {
    public:
      HostRequests Requests;

      explicit FakeHost(string_view pipeName) :
        _server(pipeName),
        _clientConnectedSubscription(_server.client_connected({ this, &FakeHost::OnClientConnected }))
      {
        _server.open();
      }

      ~FakeHost()
      {
        //The connections go before the channels they use
        lock_guard lock{ _mutex };
        _connections.clear();
      }

    private:
      named_pipe_messaging_server _server;
      event_subscription _clientConnectedSubscription;
      mutex _mutex;
      vector<unique_ptr<FakeHostConnection>> _connections;

      void OnClientConnected(messaging_server* /*server*/, messaging_channel* channel)
      {
        lock_guard lock{ _mutex };
        _connections.push_back(make_unique<FakeHostConnection>(static_cast<named_pipe_messaging_channel*>(channel), Requests));
      }
    };
  }
#pragma endregion

  TEST_CLASS(SessionToolSourceTests)
  {
    string _pipeName = format("SecondShell.Tests.Host.{}.{}", GetCurrentProcessId(), GetTickCount64());
    McpTestClient _client;
    dependency_container _container;

    static const json_object& StructuredContent(const json_value& toolResult)
    {
      return toolResult.as<json_object>().at("structuredContent")->as<json_object>();
    }

    static const json_array& Content(const json_value& toolResult)
    {
      return toolResult.as<json_object>().at("content")->as<json_array>();
    }

    static string ErrorText(const json_value& toolResult)
    {
      json_value* isError;
      Assert::IsTrue(toolResult.as<json_object>().try_get_value("isError", isError) && isError->as<json_boolean>().value, L"the tool should fail");
      return Content(toolResult).value.at(0)->as<json_object>().at("text")->as<json_string>().value;
    }

  public:
    SessionToolSourceTests()
    {
      _container.add<IMessageTransport>(_client.ServerTransport());
      _container.add<HostClient>(make_shared<HostClient>(&_container, _pipeName));
      _container.resolve<registry<ToolSource>>()->add_permanent(_container.resolve<SessionToolSource>());
      _container.resolve<McpServer>()->Start();
    }

    TEST_METHOD(ToolSchemasMatchSnapshot)
    {
      JsonSnapshot::AssertMatches("Tools", *_client.Call("tools/list", "{}"));
    }

    TEST_METHOD(WithoutHostThereIsNoSession)
    {
      auto result = _client.CallTool("Check-Session", "{}");
      JsonSnapshot::AssertEqual(R"({"session": {"state": "Stopped", "isDriverConnected": false}})", StructuredContent(*result));

      result = _client.CallTool("Get-Display", "{}");
      Assert::IsTrue(ErrorText(*result).starts_with("SessionNotActive: "));
    }

    TEST_METHOD(InitializeSessionForwardsItsSettings)
    {
      FakeHost host{ _pipeName };

      auto result = _client.CallTool("Initialize-Session", R"({"display": {"width": 1280, "height": 720, "scale": 150, "orientation": "landscape"}})");
      Assert::AreEqual(string("Ready"), StructuredContent(*result).at("session")->as<json_object>().at("state")->as<json_string>().value);

      lock_guard lock{ host.Requests.Mutex };
      Assert::AreEqual(1280u, host.Requests.Initialize->Display->Width);
      Assert::AreEqual(150u, host.Requests.Initialize->Display->Scale);
    }

    TEST_METHOD(CapturesBecomeImages)
    {
      FakeHost host{ _pipeName };

      _client.Send(R"({"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "Run-Sequence", "arguments": {"steps": [{"action": "Capture.Screen"}]}, "_meta": {"progressToken": "p"}}})");
      JsonSnapshot::AssertEqual(R"({"jsonrpc": "2.0", "method": "notifications/progress", "params": {"progressToken": "p", "progress": 1, "total": 1, "message": "Capture.Screen"}})", *_client.Receive());

      //The structured result keeps everything but the image data, also as text; the image follows with a caption
      auto response = _client.Receive();
      auto& result = *response->as<json_object>().at("result");
      auto structured = R"({"results": [{"type": "Capture", "status": "Succeeded", "tag": "0", "image": {"width": 4, "height": 2, "scale": 0.5, "format": "Png"}}]})";
      JsonSnapshot::AssertEqual(structured, StructuredContent(result));

      auto& content = Content(result).value;
      Assert::AreEqual(size_t(3), content.size());
      string_view text = content[0]->as<json_object>().at("text")->as<json_string>().value;
      JsonSnapshot::AssertEqual(structured, *json_value::from_string(text));
      JsonSnapshot::AssertEqual(R"({"type": "text", "text": "Capture 0: 4x2 pixels, scale 0.5."})", *content[1]);
      JsonSnapshot::AssertEqual(R"({"type": "image", "data": "AQID", "mimeType": "image/png"})", *content[2]);

      //The host gets a progress token of the MCP server's own
      lock_guard lock{ host.Requests.Mutex };
      Assert::IsTrue(host.Requests.Sequence.has_value(), L"the host should get the sequence");
      Assert::AreEqual(size_t(1), host.Requests.Sequence->Steps.size());
      Assert::IsTrue(host.Requests.Sequence->ProgressToken->type() == json_type::number);
    }

    TEST_METHOD(SequencesWithoutProgressSendNoToken)
    {
      FakeHost host{ _pipeName };

      auto result = _client.CallTool("Run-Sequence", R"({"steps": [{"action": "Capture.Screen"}]})");

      lock_guard lock{ host.Requests.Mutex };
      Assert::IsTrue(host.Requests.Sequence.has_value(), to_wstring(result->to_string()).c_str());
      Assert::IsFalse(bool(host.Requests.Sequence->ProgressToken));
    }

    TEST_METHOD(TheProgressTokenIsNotAnArgument)
    {
      auto result = _client.CallTool("Run-Sequence", R"({"steps": [{"action": "Capture.Screen"}], "progressToken": 1})");
      Assert::IsTrue(ErrorText(*result).starts_with("Invalid arguments: unknown property 'progressToken'"));
    }

    TEST_METHOD(HostErrorsBecomeToolErrors)
    {
      FakeHost host{ _pipeName };

      auto result = _client.CallTool("Configure-Capture", R"({"capture": {"format": "Jpeg", "inlineLimit": 2}})");
      Assert::AreEqual(string("AgentInputDisabled: The user has taken control."), ErrorText(*result));
    }
  };
}
