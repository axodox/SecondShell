#include "pch.h"
#include "JsonRpcPeer.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Axodox::Threading;
using namespace std;

namespace SecondShell::Rpc
{
  namespace
  {
    //MCP's cancellation notification, used on every link.
    struct CancelledNotification
    {
      static json_object_descriptor<CancelledNotification> json_description;

      value_ptr<json_value> RequestId;
      string Reason;
    };

    json_object_descriptor<CancelledNotification> CancelledNotification::json_description = describe_json_object<CancelledNotification>(
      { .name = "notifications/cancelled" },
      {
        { &CancelledNotification::RequestId, "requestId", { .is_required = true } },
        { &CancelledNotification::Reason, "reason", { .is_required = false } }
      });

    //The key of a running request: its ID as JSON text, so number and string IDs are told apart
    string RequestKey(const json_value* id)
    {
      return id ? id->to_string() : string{};
    }
  }

  struct JsonRpcPeer::PendingRequest
  {
    manual_reset_event Completed;
    optional<JsonRpcMessage> Response;
  };

#pragma region Setup
  JsonRpcPeer::JsonRpcPeer(shared_ptr<IMessageTransport> transport) :
    _transport(move(transport))
  { }

  JsonRpcPeer::~JsonRpcPeer()
  {
    //Stop receiving, then stop the work in progress; _lifetime waits for the running requests to return
    _messageReceivedSubscription.reset();
    _closedSubscription.reset();
    CloseAll();
  }

  void JsonRpcPeer::RegisterRequest(const char* method, RequestHandler&& handler)
  {
    if (_isStarted) throw logic_error("Handlers must be added before the peer is started.");
    _requestHandlers[method] = move(handler);
  }

  void JsonRpcPeer::RegisterNotification(const char* method, NotificationHandler&& handler)
  {
    if (_isStarted) throw logic_error("Handlers must be added before the peer is started.");
    _notificationHandlers[method] = move(handler);
  }

  void JsonRpcPeer::Start()
  {
    if (_isStarted) return;
    _isStarted = true;

    _messageReceivedSubscription = _transport->MessageReceived({ this, &JsonRpcPeer::OnMessageReceived });
    _closedSubscription = _transport->Closed({ this, &JsonRpcPeer::OnClosed });

    if (!_transport->IsOpen()) CloseAll();
  }

  IMessageTransport* JsonRpcPeer::Transport() const
  {
    return _transport.get();
  }
#pragma endregion

#pragma region Sending
  RpcResult<JsonRpcPeer::json_value_ptr> JsonRpcPeer::SendRequest(const char* method, json_value_ptr&& params, chrono::milliseconds timeout, const CancellationToken& cancellation)
  {
    //Register the request
    auto pending = make_shared<PendingRequest>();
    int64_t id;
    {
      lock_guard lock{ _mutex };
      if (_isClosed) return JsonRpcError{ JsonRpcErrorCode::ConnectionClosed, "The connection is closed." };

      id = _nextRequestId++;
      _pendingRequests[id] = pending;
    }

    //Send it, and wait for the response, the timeout or cancellation
    auto cancelledSubscription = cancellation.Cancelled([&](CancellationToken*) { pending->Completed.set(); });
    try
    {
      _transport->Send(JsonRpcMessage{ .Id = make_value<json_number>(double(id)), .Method = method, .Params = move(params) }.ToString());
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::warning, "Could not send request {}: {}", method, exception.what());
      lock_guard lock{ _mutex };
      _pendingRequests.erase(id);
      return JsonRpcError{ JsonRpcErrorCode::ConnectionClosed, "The connection is closed." };
    }

    if (!cancellation.IsCancelled()) pending->Completed.wait(timeout);

    optional<JsonRpcMessage> response;
    {
      lock_guard lock{ _mutex };
      _pendingRequests.erase(id);
      response = move(pending->Response);
    }

    //Answered
    if (response)
    {
      if (response->Error) return *response->Error;
      return response->Result ? response->Result : json_value_ptr(make_value<json_null>());
    }

    //Not answered: tell the other side to stop working on it
    auto isCancelled = cancellation.IsCancelled();
    SendNotification(CancelledNotification::json_description.name(), json_serializer<CancelledNotification>::to_json(CancelledNotification{ make_value<json_number>(double(id)), isCancelled ? "Cancelled" : "Timeout" }));

    if (isCancelled) return JsonRpcError{ JsonRpcErrorCode::RequestCancelled, format("Request {} was cancelled.", method) };
    return JsonRpcError{ JsonRpcErrorCode::Timeout, format("Request {} was not answered within {} ms.", method, timeout.count()) };
  }

  void JsonRpcPeer::SendNotification(const char* method, json_value_ptr&& params)
  {
    SendMessage(JsonRpcMessage{ .Method = method, .Params = move(params) });
  }

  void JsonRpcPeer::SendMessage(const JsonRpcMessage& message)
  {
    try
    {
      _transport->Send(message.ToString());
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::warning, "Could not send message: {}", exception.what());
    }
  }
#pragma endregion

#pragma region Receiving
  void JsonRpcPeer::OnMessageReceived(IMessageTransport* /*transport*/, string_view text)
  {
    auto message = JsonRpcMessage::Parse(text);
    if (!message)
    {
      _logger.log(log_severity::warning, "Received an invalid message: {}", message.error().Message);
      SendMessage(JsonRpcMessage{ .Error = message.error() });
      return;
    }

    auto parsed = *message;
    switch (parsed.Kind())
    {
    case JsonRpcMessageKind::Request:
      StartRequest(move(parsed));
      break;
    case JsonRpcMessageKind::Notification:
      HandleNotification(parsed);
      break;
    case JsonRpcMessageKind::Response:
      HandleResponse(move(parsed));
      break;
    }
  }

  void JsonRpcPeer::OnClosed(IMessageTransport* /*transport*/)
  {
    CloseAll();
  }

  void JsonRpcPeer::StartRequest(JsonRpcMessage&& request)
  {
    auto handler = _requestHandlers.find(request.Method);
    if (handler == _requestHandlers.end())
    {
      SendMessage(JsonRpcMessage{ .Id = request.Id, .Error = JsonRpcError{ JsonRpcErrorCode::MethodNotFound, format("Unknown method {}.", request.Method) } });
      return;
    }

    auto guard = _lifetime.try_guard();
    if (!guard) return;

    auto cancellation = make_shared<CancellationToken>();
    {
      lock_guard lock{ _mutex };
      _runningRequests[RequestKey(request.Id.get())] = cancellation;
    }

    thread([this, guard = move(guard), request = move(request), handler = &handler->second, cancellation] {
      RunRequest(request, *handler, *cancellation);
      }).detach();
  }

  void JsonRpcPeer::RunRequest(const JsonRpcMessage& request, const RequestHandler& handler, CancellationToken& cancellation)
  {
    set_thread_name("* JSON-RPC " + request.Method);

    //Run the handler; exceptions become errors, they never leave the thread
    RpcResult<json_value_ptr> result;
    try
    {
      result = handler(request.Params.get(), cancellation);
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::error, "Request {} failed: {}", request.Method, exception.what());
      result = JsonRpcError{ JsonRpcErrorCode::InternalError, exception.what() };
    }
    catch (const winrt::hresult_error& error)
    {
      auto message = winrt::to_string(error.message());
      _logger.log(log_severity::error, "Request {} failed: {}", request.Method, message);
      result = JsonRpcError{ JsonRpcErrorCode::InternalError, message };
    }

    {
      lock_guard lock{ _mutex };
      _runningRequests.erase(RequestKey(request.Id.get()));
    }

    //The requester of a cancelled request has stopped waiting, as MCP specifies
    if (cancellation.IsCancelled()) return;

    if (auto error = get_if<JsonRpcError>(&result))
    {
      SendMessage(JsonRpcMessage{ .Id = request.Id, .Error = *error });
    }
    else
    {
      SendMessage(JsonRpcMessage{ .Id = request.Id, .Result = get<json_value_ptr>(result) });
    }
  }

  void JsonRpcPeer::HandleNotification(const JsonRpcMessage& notification)
  {
    if (notification.Method == CancelledNotification::json_description.name())
    {
      CancelRunningRequest(notification.Params.get());
      return;
    }

    auto handler = _notificationHandlers.find(notification.Method);
    if (handler == _notificationHandlers.end()) return;

    try
    {
      handler->second(notification.Params.get());
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::error, "Notification {} failed: {}", notification.Method, exception.what());
    }
  }

  void JsonRpcPeer::HandleResponse(JsonRpcMessage&& response)
  {
    auto id = response.Id ? response.Id->try_as<json_number>() : nullptr;
    if (!id)
    {
      _logger.log(log_severity::warning, "Received a response without a request ID: {}", response.Error ? response.Error->Message : "");
      return;
    }

    lock_guard lock{ _mutex };
    auto pending = _pendingRequests.find(int64_t(id->value));
    if (pending == _pendingRequests.end()) return;

    pending->second->Response = move(response);
    pending->second->Completed.set();
  }

  void JsonRpcPeer::CancelRunningRequest(const json_value* params)
  {
    auto notification = ParseParams<CancelledNotification>(params, JsonRpcErrorCode::InvalidParams);
    if (holds_alternative<JsonRpcError>(notification)) return;

    shared_ptr<CancellationToken> cancellation;
    {
      lock_guard lock{ _mutex };
      auto running = _runningRequests.find(RequestKey(get<CancelledNotification>(notification).RequestId.get()));
      if (running != _runningRequests.end()) cancellation = running->second;
    }

    if (cancellation) cancellation->Cancel();
  }

  void JsonRpcPeer::CloseAll()
  {
    //Fail the requests waiting for a response, and cancel the requests being handled
    vector<shared_ptr<CancellationToken>> runningRequests;
    {
      lock_guard lock{ _mutex };
      _isClosed = true;

      for (auto& [id, pending] : _pendingRequests)
      {
        pending->Response = JsonRpcMessage{ .Error = JsonRpcError{ JsonRpcErrorCode::ConnectionClosed, "The connection was closed." } };
        pending->Completed.set();
      }

      for (auto& [id, cancellation] : _runningRequests)
      {
        runningRequests.push_back(cancellation);
      }
    }

    for (auto& cancellation : runningRequests)
    {
      cancellation->Cancel();
    }
  }
#pragma endregion
}
