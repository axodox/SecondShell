#pragma once
#include "ActionEnums.h"
#include "JsonRpcError.h"

namespace SecondShell::Protocol
{
  //SecondShell's errors (Design.md, Errors and timeouts) as JSON-RPC errors, with codes in the application range
  //(1000 + the ErrorCode value), so the MCP server can report them by name.
  struct SECONDSHELL_COMMON_API ProtocolErrors
  {
    static Rpc::JsonRpcError Make(Actions::ErrorCode code, std::string message);
    static std::optional<Actions::ErrorCode> TryGetCode(const Rpc::JsonRpcError& error);
  };
}
