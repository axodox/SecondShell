#pragma once
#include "CommonApi.h"

namespace SecondShell::Rpc
{
  //The error codes of JSON-RPC 2.0 and of the peer itself; applications define their own codes as well.
  enum class JsonRpcErrorCode : int32_t
  {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ConnectionClosed = -32000,
    Timeout = -32001,
    RequestCancelled = -32800
  };

  struct SECONDSHELL_COMMON_API JsonRpcError
  {
    static Axodox::Json::json_object_descriptor<JsonRpcError> json_description;

    int32_t Code = 0;
    std::string Message;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Data;

    JsonRpcError() = default;
    JsonRpcError(JsonRpcErrorCode code, std::string message);
    JsonRpcError(int32_t code, std::string message);
  };

  //Text form of an error, used by Axodox::Infrastructure::expected<T, JsonRpcError>::check.
  SECONDSHELL_COMMON_API std::string format_error(const JsonRpcError& error);

  //The outcome of a request: its response, or the error it failed with.
  template<typename TResponse>
  using RpcResult = std::variant<TResponse, JsonRpcError>;
}
