#include "pch.h"
#include "JsonRpcMessage.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace std;

namespace SecondShell::Rpc
{
  const char* const _version = "2.0";

  namespace
  {
    failure<JsonRpcError> InvalidRequest(string_view reason)
    {
      return failure<JsonRpcError>(JsonRpcError{ JsonRpcErrorCode::InvalidRequest, format("Invalid JSON-RPC message: {}", reason) });
    }

    bool IsValidId(const json_value* id)
    {
      return id && (id->type() == json_type::number || id->type() == json_type::string);
    }

    bool IsValidParams(const json_value* params)
    {
      return params && (params->type() == json_type::object || params->type() == json_type::array);
    }

    value_ptr<json_value> NullIfEmpty(const value_ptr<json_value>& value)
    {
      return value ? value : value_ptr<json_value>(make_value<json_null>());
    }
  }

  JsonRpcMessageKind JsonRpcMessage::Kind() const
  {
    if (Method.empty()) return JsonRpcMessageKind::Response;

    return Id ? JsonRpcMessageKind::Request : JsonRpcMessageKind::Notification;
  }

  string JsonRpcMessage::ToString() const
  {
    json_object json;
    json.set_value("jsonrpc", _version);

    if (Kind() == JsonRpcMessageKind::Response)
    {
      //A response always has an ID, null if the request's ID could not be read
      json.set_value("id", NullIfEmpty(Id));

      if (Error) json.set_value("error", *Error);
      else json.set_value("result", NullIfEmpty(Result));
    }
    else
    {
      if (Id) json.set_value("id", Id);
      json.set_value("method", Method);
      if (Params) json.set_value("params", Params);
    }

    return json.to_string();
  }

  expected<JsonRpcMessage, JsonRpcError> JsonRpcMessage::Parse(string_view text)
  {
    //Parse the JSON
    auto json = json_value::from_string(text);
    if (!json) return failure<JsonRpcError>(JsonRpcError{ JsonRpcErrorCode::ParseError, "The message is not valid JSON." });

    auto object = json->try_as<json_object>();
    if (!object) return InvalidRequest("not an object");

    string version;
    if (!object->try_get_value("jsonrpc", version) || version != _version) return InvalidRequest("jsonrpc must be \"2.0\"");

    JsonRpcMessage result;
    json_value* value;

    //Request or notification
    if (object->try_get_value("method", value))
    {
      auto method = value ? value->try_as<json_string>() : nullptr;
      if (!method || method->value.empty()) return InvalidRequest("method must be a non-empty string");
      result.Method = method->value;

      if (object->try_get_value("id", value))
      {
        if (!IsValidId(value)) return InvalidRequest("id must be a number or a string");
        result.Id = object->at("id");
      }

      if (object->try_get_value("params", value) && value && value->type() != json_type::null)
      {
        if (!IsValidParams(value)) return InvalidRequest("params must be an object or an array");
        result.Params = object->at("params");
      }

      return result;
    }

    //Response
    if (object->try_get_value("id", value) && value && value->type() != json_type::null)
    {
      if (!IsValidId(value)) return InvalidRequest("id must be a number or a string");
      result.Id = object->at("id");
    }

    if (object->try_get_value("error", value))
    {
      JsonRpcError error;
      if (!JsonRpcError::json_description.from_json(error, value)) return InvalidRequest("error must be an object with a code and a message");
      result.Error = move(error);
      return result;
    }

    if (object->try_get_value("result", value))
    {
      result.Result = object->at("result");
      return result;
    }

    return InvalidRequest("it is neither a request, a notification nor a response");
  }
}
