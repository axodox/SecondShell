#include "pch.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Protocol
{
  const int32_t _firstErrorCode = 1000;

  JsonRpcError ProtocolErrors::Make(ErrorCode code, string message)
  {
    return JsonRpcError{ _firstErrorCode + int32_t(code), move(message) };
  }

  optional<ErrorCode> ProtocolErrors::TryGetCode(const JsonRpcError& error)
  {
    auto value = error.Code - _firstErrorCode;
    for (auto& item : enum_values<ErrorCode>())
    {
      if (int32_t(item.value) == value) return item.value;
    }

    return nullopt;
  }

  string ProtocolErrors::ToText(const JsonRpcError& error)
  {
    auto code = error.Code == int32_t(JsonRpcErrorCode::Timeout) ? ErrorCode::Timeout : TryGetCode(error);
    return code ? format("{}: {}", to_string(*code), error.Message) : error.Message;
  }
}
