#include "pch.h"
#include "JsonRpcPeer.h"
#include "MemoryTransport.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Axodox::Threading;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
#pragma region Test protocol
  namespace
  {
    struct EmptyResponse
    {
      static json_object_descriptor<EmptyResponse> json_description;
    };

    struct EchoResponse
    {
      static json_object_descriptor<EchoResponse> json_description;

      string Text;
    };

    struct EchoRequest
    {
      static json_object_descriptor<EchoRequest> json_description;
      using ResponseType = EchoResponse;

      string Text;
    };

    //The same method without the required property, to send invalid params
    struct EchoRequestWithoutText
    {
      static json_object_descriptor<EchoRequestWithoutText> json_description;
      using ResponseType = EchoResponse;
    };

    struct FailRequest
    {
      static json_object_descriptor<FailRequest> json_description;
      using ResponseType = EchoResponse;
    };

    struct ThrowRequest
    {
      static json_object_descriptor<ThrowRequest> json_description;
      using ResponseType = EchoResponse;
    };

    struct WaitRequest
    {
      static json_object_descriptor<WaitRequest> json_description;
      using ResponseType = EmptyResponse;

      uint32_t Milliseconds = 0;
    };

    struct UnknownRequest
    {
      static json_object_descriptor<UnknownRequest> json_description;
      using ResponseType = EmptyResponse;
    };

    struct NoteNotification
    {
      static json_object_descriptor<NoteNotification> json_description;

      string Text;
    };

    json_object_descriptor<EmptyResponse> EmptyResponse::json_description = describe_json_object<EmptyResponse>({});
    json_object_descriptor<EchoResponse> EchoResponse::json_description = describe_json_object<EchoResponse>({ { &EchoResponse::Text, "text" } });
    json_object_descriptor<EchoRequest> EchoRequest::json_description = describe_json_object<EchoRequest>({ .name = "test/echo" }, { { &EchoRequest::Text, "text", { .is_required = true } } });
    json_object_descriptor<EchoRequestWithoutText> EchoRequestWithoutText::json_description = describe_json_object<EchoRequestWithoutText>({ .name = "test/echo" }, {});
    json_object_descriptor<FailRequest> FailRequest::json_description = describe_json_object<FailRequest>({ .name = "test/fail" }, {});
    json_object_descriptor<ThrowRequest> ThrowRequest::json_description = describe_json_object<ThrowRequest>({ .name = "test/throw" }, {});
    json_object_descriptor<WaitRequest> WaitRequest::json_description = describe_json_object<WaitRequest>({ .name = "test/wait" }, { { &WaitRequest::Milliseconds, "milliseconds" } });
    json_object_descriptor<UnknownRequest> UnknownRequest::json_description = describe_json_object<UnknownRequest>({ .name = "test/unknown" }, {});
    json_object_descriptor<NoteNotification> NoteNotification::json_description = describe_json_object<NoteNotification>({ .name = "test/note" }, { { &NoteNotification::Text, "text" } });

    class TestService
    {
    public:
      manual_reset_event WaitStarted;
      atomic_bool WasCancelled = false;
      mutex NotesMutex;
      vector<string> Notes;

      RpcResult<EchoResponse> Echo(const EchoRequest& request, const CancellationToken& /*cancellation*/)
      {
        return EchoResponse{ request.Text };
      }

      RpcResult<EchoResponse> Fail(const FailRequest& /*request*/, const CancellationToken& /*cancellation*/)
      {
        return JsonRpcError{ 1234, "Failed on purpose." };
      }

      RpcResult<EchoResponse> Throw(const ThrowRequest& /*request*/, const CancellationToken& /*cancellation*/)
      {
        throw runtime_error("Thrown on purpose.");
      }

      RpcResult<EmptyResponse> Wait(const WaitRequest& request, const CancellationToken& cancellation)
      {
        WaitStarted.set();
        WasCancelled = cancellation.WaitFor(chrono::milliseconds(request.Milliseconds));
        return EmptyResponse{};
      }

      void Note(const NoteNotification& notification)
      {
        lock_guard lock{ NotesMutex };
        Notes.push_back(notification.Text);
      }
    };
  }
#pragma endregion

  TEST_CLASS(JsonRpcPeerTests)
  {
    static constexpr auto _timeout = 5s;

    shared_ptr<MemoryTransport> _clientTransport;
    shared_ptr<MemoryTransport> _serverTransport;
    TestService _clientService;
    TestService _serverService;
    unique_ptr<JsonRpcPeer> _client;
    unique_ptr<JsonRpcPeer> _server;
    CancellationToken _noCancellation;

    static void AddHandlers(JsonRpcPeer& peer, TestService& service)
    {
      peer.AddHandler(&service, &TestService::Echo);
      peer.AddHandler(&service, &TestService::Fail);
      peer.AddHandler(&service, &TestService::Throw);
      peer.AddHandler(&service, &TestService::Wait);
      peer.AddNotificationHandler(&service, &TestService::Note);
    }

    template<typename TResponse>
    static JsonRpcError ExpectError(const RpcResult<TResponse>& result)
    {
      Assert::IsTrue(holds_alternative<JsonRpcError>(result), L"the request should fail");
      return get<JsonRpcError>(result);
    }

    template<typename TResponse>
    static TResponse ExpectResponse(const RpcResult<TResponse>& result)
    {
      if (auto error = get_if<JsonRpcError>(&result)) Assert::Fail(Axodox::Infrastructure::to_wstring(error->Message).c_str());
      return get<TResponse>(result);
    }

    static bool WaitUntil(const function<bool()>& condition)
    {
      auto deadline = chrono::steady_clock::now() + _timeout;
      while (!condition() && chrono::steady_clock::now() < deadline) this_thread::sleep_for(10ms);
      return condition();
    }

  public:
    TEST_METHOD_INITIALIZE(Connect)
    {
      tie(_clientTransport, _serverTransport) = MemoryTransport::CreatePair();

      _server = make_unique<JsonRpcPeer>(_serverTransport);
      AddHandlers(*_server, _serverService);
      _server->Start();

      _client = make_unique<JsonRpcPeer>(_clientTransport);
      AddHandlers(*_client, _clientService);
      _client->Start();
    }

    TEST_METHOD(RequestReturnsResponse)
    {
      auto response = ExpectResponse(_client->Request(EchoRequest{ "hello" }, _timeout, _noCancellation));
      Assert::AreEqual(string("hello"), response.Text);
    }

    TEST_METHOD(BothSidesCanRequest)
    {
      auto response = ExpectResponse(_server->Request(EchoRequest{ "from the server" }, _timeout, _noCancellation));
      Assert::AreEqual(string("from the server"), response.Text);
    }

    TEST_METHOD(HandlerErrorIsReturned)
    {
      auto error = ExpectError(_client->Request(FailRequest{}, _timeout, _noCancellation));
      Assert::AreEqual(1234, error.Code);
      Assert::AreEqual(string("Failed on purpose."), error.Message);
    }

    TEST_METHOD(HandlerExceptionBecomesInternalError)
    {
      auto error = ExpectError(_client->Request(ThrowRequest{}, _timeout, _noCancellation));
      Assert::AreEqual(int32_t(JsonRpcErrorCode::InternalError), error.Code);
      Assert::AreEqual(string("Thrown on purpose."), error.Message);
    }

    TEST_METHOD(UnknownMethodIsReported)
    {
      auto error = ExpectError(_client->Request(UnknownRequest{}, _timeout, _noCancellation));
      Assert::AreEqual(int32_t(JsonRpcErrorCode::MethodNotFound), error.Code);
    }

    TEST_METHOD(InvalidParamsAreRejectedBeforeTheHandler)
    {
      auto error = ExpectError(_client->Request(EchoRequestWithoutText{}, _timeout, _noCancellation));
      Assert::AreEqual(int32_t(JsonRpcErrorCode::InvalidParams), error.Code);
      Assert::IsTrue(error.Message.find("text") != string::npos, L"the message should name the missing property");
    }

    TEST_METHOD(NotificationIsDelivered)
    {
      _client->Notify(NoteNotification{ "first" });
      _client->Notify(NoteNotification{ "second" });

      Assert::IsTrue(WaitUntil([&] { lock_guard lock{ _serverService.NotesMutex }; return _serverService.Notes.size() == 2; }), L"both notifications should arrive");
      lock_guard lock{ _serverService.NotesMutex };
      Assert::AreEqual(string("first"), _serverService.Notes[0]);
      Assert::AreEqual(string("second"), _serverService.Notes[1]);
    }

    TEST_METHOD(TimeoutCancelsTheRemoteHandler)
    {
      auto error = ExpectError(_client->Request(WaitRequest{ 10000 }, 200ms, _noCancellation));
      Assert::AreEqual(int32_t(JsonRpcErrorCode::Timeout), error.Code);

      Assert::IsTrue(WaitUntil([&] { return _serverService.WasCancelled.load(); }), L"the handler should be cancelled");
    }

    TEST_METHOD(CallerCancellationCancelsTheRemoteHandler)
    {
      CancellationToken cancellation;
      thread canceller([&] {
        _serverService.WaitStarted.wait(_timeout);
        cancellation.Cancel();
        });

      auto error = ExpectError(_client->Request(WaitRequest{ 10000 }, _timeout, cancellation));
      canceller.join();

      Assert::AreEqual(int32_t(JsonRpcErrorCode::RequestCancelled), error.Code);
      Assert::IsTrue(WaitUntil([&] { return _serverService.WasCancelled.load(); }), L"the handler should be cancelled");
    }

    TEST_METHOD(LongRequestDoesNotBlockOthers)
    {
      auto longRequest = async(launch::async, [&] { return _client->Request(WaitRequest{ 3000 }, _timeout, _noCancellation); });
      Assert::IsTrue(_serverService.WaitStarted.wait(_timeout));

      auto start = chrono::steady_clock::now();
      auto response = ExpectResponse(_client->Request(EchoRequest{ "meanwhile" }, _timeout, _noCancellation));
      Assert::AreEqual(string("meanwhile"), response.Text);
      Assert::IsTrue(chrono::steady_clock::now() - start < 1s, L"the echo should not wait for the long request");

      ExpectResponse(longRequest.get());
    }

    TEST_METHOD(ParallelRequestsGetTheirOwnResponses)
    {
      vector<future<RpcResult<EchoResponse>>> requests;
      for (auto i = 0; i < 50; i++)
      {
        requests.push_back(async(launch::async, [&, i] { return _client->Request(EchoRequest{ to_string(i) }, _timeout, _noCancellation); }));
      }

      for (auto i = 0; i < 50; i++)
      {
        Assert::AreEqual(to_string(i), ExpectResponse(requests[i].get()).Text);
      }
    }

    TEST_METHOD(ClosedConnectionFailsPendingAndNewRequests)
    {
      auto pendingRequest = async(launch::async, [&] { return _client->Request(WaitRequest{ 10000 }, _timeout, _noCancellation); });
      Assert::IsTrue(_serverService.WaitStarted.wait(_timeout));

      _clientTransport->Close();

      Assert::AreEqual(int32_t(JsonRpcErrorCode::ConnectionClosed), ExpectError(pendingRequest.get()).Code);
      Assert::AreEqual(int32_t(JsonRpcErrorCode::ConnectionClosed), ExpectError(_client->Request(EchoRequest{ "late" }, _timeout, _noCancellation)).Code);
      Assert::IsTrue(WaitUntil([&] { return _serverService.WasCancelled.load(); }), L"the server should cancel the handler of the closed connection");
    }

    TEST_METHOD(HandlersCannotBeAddedAfterStart)
    {
      TestService service;
      Assert::ExpectException<logic_error>([&] { _client->AddHandler(&service, &TestService::Echo); });
    }
  };
}
