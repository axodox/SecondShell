#pragma once
#include "CancellationToken.h"
#include "IMessageTransport.h"
#include "JsonRpcMessage.h"

namespace SecondShell::Rpc
{
  //Both ends of a JSON-RPC connection: answers incoming requests with registered handlers, and sends requests and
  //notifications of its own. A method is declared by its described request type: the descriptor name is the method
  //name, the descriptor validates the params, and the request type names its response type as ResponseType.
  //
  //Incoming requests run on their own threads, so a long request does not hold up others; notifications and responses
  //are handled on the transport's thread, in order, so a notification handler must not wait for a response of the same
  //peer. "notifications/cancelled" (as in MCP) cancels a running request on either side.
  class SECONDSHELL_COMMON_API JsonRpcPeer
  {
    inline static const Axodox::Infrastructure::logger _logger{ "JsonRpcPeer" };

    using json_value_ptr = Axodox::Infrastructure::value_ptr<Axodox::Json::json_value>;
    using RequestHandler = std::function<RpcResult<json_value_ptr>(const Axodox::Json::json_value* params, const CancellationToken& cancellation)>;
    using NotificationHandler = std::function<void(const Axodox::Json::json_value* params)>;

    struct PendingRequest;

  public:
    explicit JsonRpcPeer(std::shared_ptr<IMessageTransport> transport);
    ~JsonRpcPeer();

    JsonRpcPeer(const JsonRpcPeer&) = delete;
    JsonRpcPeer& operator=(const JsonRpcPeer&) = delete;

    //Handlers are added before Start.
    template<typename TOwner, typename TRequest, typename TResponse>
    void AddHandler(TOwner* owner, RpcResult<TResponse>(TOwner::* handler)(const TRequest& request, const CancellationToken& cancellation))
    {
      RegisterRequest(TRequest::json_description.name(), [owner, handler](const Axodox::Json::json_value* params, const CancellationToken& cancellation) -> RpcResult<json_value_ptr> {
        auto request = ParseParams<TRequest>(params, JsonRpcErrorCode::InvalidParams);
        if (auto error = std::get_if<JsonRpcError>(&request)) return *error;

        auto response = (owner->*handler)(std::get<TRequest>(request), cancellation);
        if (auto error = std::get_if<JsonRpcError>(&response)) return *error;

        return Axodox::Json::json_serializer<TResponse>::to_json(std::get<TResponse>(response));
        });
    }

    template<typename TOwner, typename TNotification>
    void AddNotificationHandler(TOwner* owner, void(TOwner::* handler)(const TNotification& notification))
    {
      RegisterNotification(TNotification::json_description.name(), [owner, handler](const Axodox::Json::json_value* params) {
        auto notification = ParseParams<TNotification>(params, JsonRpcErrorCode::InvalidParams);
        if (auto error = std::get_if<JsonRpcError>(&notification))
        {
          _logger.log(Axodox::Infrastructure::log_severity::warning, "Ignored notification {}: {}", TNotification::json_description.name(), error->Message);
          return;
        }

        (owner->*handler)(std::get<TNotification>(notification));
        });
    }

    //Sends a request and waits for its response until the timeout expires (zero: no time limit) or the request is cancelled.
    template<typename TRequest, typename TResponse = typename TRequest::ResponseType>
    RpcResult<TResponse> Request(const TRequest& request, std::chrono::milliseconds timeout, const CancellationToken& cancellation)
    {
      auto result = SendRequest(TRequest::json_description.name(), Axodox::Json::json_serializer<TRequest>::to_json(request), timeout, cancellation);
      if (auto error = std::get_if<JsonRpcError>(&result)) return *error;

      return ParseParams<TResponse>(std::get<json_value_ptr>(result).get(), JsonRpcErrorCode::InternalError);
    }

    template<typename TNotification>
    void Notify(const TNotification& notification)
    {
      SendNotification(TNotification::json_description.name(), Axodox::Json::json_serializer<TNotification>::to_json(notification));
    }

    void Start();
    IMessageTransport* Transport() const;

  private:
    std::shared_ptr<IMessageTransport> _transport;
    std::unordered_map<std::string, RequestHandler> _requestHandlers;
    std::unordered_map<std::string, NotificationHandler> _notificationHandlers;
    bool _isStarted = false;

    std::mutex _mutex;
    bool _isClosed = false;
    int64_t _nextRequestId = 1;
    std::unordered_map<int64_t, std::shared_ptr<PendingRequest>> _pendingRequests;
    std::unordered_map<std::string, std::shared_ptr<CancellationToken>> _runningRequests;

    Axodox::Infrastructure::event_subscription _messageReceivedSubscription;
    Axodox::Infrastructure::event_subscription _closedSubscription;
    Axodox::Threading::lifetime_source _lifetime;

    //Converts params or a result to a described type; failures become errors with the given code.
    template<typename TValue>
    static std::variant<TValue, JsonRpcError> ParseParams(const Axodox::Json::json_value* json, JsonRpcErrorCode errorCode)
    {
      Axodox::Json::json_object empty;
      if (!json || json->type() == Axodox::Json::json_type::null) json = &empty;

      auto name = TValue::json_description.name();
      if (!name) name = "response";

      auto validation = TValue::json_description.validate(*json);
      if (!validation) return JsonRpcError{ errorCode, std::format("Invalid {}: {}", name, validation.error()) };

      TValue result{};
      auto parsing = TValue::json_description.from_json(result, json);
      if (!parsing) return JsonRpcError{ errorCode, std::format("Invalid {}: {}", name, parsing.error()) };

      return result;
    }

    void RegisterRequest(const char* method, RequestHandler&& handler);
    void RegisterNotification(const char* method, NotificationHandler&& handler);

    RpcResult<json_value_ptr> SendRequest(const char* method, json_value_ptr&& params, std::chrono::milliseconds timeout, const CancellationToken& cancellation);
    void SendNotification(const char* method, json_value_ptr&& params);
    void SendMessage(const JsonRpcMessage& message);

    void OnMessageReceived(IMessageTransport* transport, std::string_view text);
    void OnClosed(IMessageTransport* transport);

    void StartRequest(JsonRpcMessage&& request);
    void RunRequest(const JsonRpcMessage& request, const RequestHandler& handler, CancellationToken& cancellation);
    void HandleNotification(const JsonRpcMessage& notification);
    void HandleResponse(JsonRpcMessage&& response);
    void CancelRunningRequest(const Axodox::Json::json_value* params);
    void CloseAll();
  };
}
