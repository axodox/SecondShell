#include "pch.h"
#include "McpProbe.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

namespace SecondShell::Tests
{
  //Runs the real MCP server; expects that no SecondShell host is running for the current user.
  TEST_CLASS(McpProbeTests)
  {
  public:
    TEST_METHOD(ServesToolsOverStdio)
    {
      McpProbe probe;

      probe.Send(R"({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "probe", "version": "1"}}})");
      auto initialize = probe.Receive()->as<json_object>().at("result");
      Assert::AreEqual(string("2025-11-25"), initialize->as<json_object>().at("protocolVersion")->as<json_string>().value);
      probe.Send(R"({"jsonrpc": "2.0", "method": "notifications/initialized"})");

      //The seven tools
      probe.Send(R"({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})");
      auto list = probe.Receive()->as<json_object>().at("result");
      vector<string> names;
      for (auto& tool : list->as<json_object>().at("tools")->as<json_array>().value)
      {
        names.push_back(tool->as<json_object>().at("name")->as<json_string>().value);
      }
      Assert::IsTrue(names == vector<string>{ "Initialize-Session", "Check-Session", "Shutdown-Session", "Run-Sequence", "Set-Display", "Get-Display", "Configure-Capture" });

      //Without a host, calls fail with a clear error
      probe.Send(R"({"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "Get-Display", "arguments": {}}})");
      auto call = probe.Receive()->as<json_object>().at("result");
      auto& result = call->as<json_object>();
      Assert::IsTrue(result.at("isError")->as<json_boolean>().value);
      auto text = result.at("content")->as<json_array>().value.at(0)->as<json_object>().at("text")->as<json_string>().value;
      Assert::IsTrue(text.starts_with("SessionNotActive: SecondShell is not running"), to_wstring(text).c_str());

      Assert::IsTrue(probe.Close() == 0u, L"the server should exit when stdin closes");
    }
  };
}
