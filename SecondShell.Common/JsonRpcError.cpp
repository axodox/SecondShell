#include "pch.h"
#include "JsonRpcError.h"

using namespace Axodox::Json;
using namespace std;

namespace SecondShell::Rpc
{
  json_object_descriptor<JsonRpcError> JsonRpcError::json_description = describe_json_object<JsonRpcError>({
    { &JsonRpcError::Code, "code" },
    { &JsonRpcError::Message, "message" },
    { &JsonRpcError::Data, "data", { .is_required = false } }
    });

  JsonRpcError::JsonRpcError(JsonRpcErrorCode code, string message) :
    JsonRpcError(int32_t(code), move(message))
  { }

  JsonRpcError::JsonRpcError(int32_t code, string message) :
    Code(code),
    Message(move(message))
  { }

  string format_error(const JsonRpcError& error)
  {
    return format("{} (code {})", error.Message, error.Code);
  }
}
