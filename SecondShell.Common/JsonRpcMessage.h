#pragma once
#include "JsonRpcError.h"

namespace SecondShell::Rpc
{
  enum class JsonRpcMessageKind
  {
    Request,
    Notification,
    Response
  };

  //A JSON-RPC 2.0 message. Params and result stay raw JSON here; the peer converts them to the described
  //request and response types. The envelope is written by hand, because its fields depend on the kind of message:
  //a request has an ID and a notification does not, and a response has either a result or an error.
  struct SECONDSHELL_COMMON_API JsonRpcMessage
  {
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Id;  //number or string; requests and responses
    std::string Method;                                               //requests and notifications
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Params;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Result;
    std::optional<JsonRpcError> Error;

    JsonRpcMessageKind Kind() const;
    std::string ToString() const;

    //Fails with ParseError for text which is not JSON, and with InvalidRequest for JSON which is not a JSON-RPC 2.0 message.
    static Axodox::Infrastructure::expected<JsonRpcMessage, JsonRpcError> Parse(std::string_view text);
  };
}
