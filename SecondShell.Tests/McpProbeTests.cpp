#include "pch.h"
#include "AppEnvironment.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Axodox::Threading;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Environment;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  namespace
  {
    //Runs SecondShell.Mcp.exe and speaks MCP with it over stdio, as an AI client does.
    class McpProbe
    {
    public:
      McpProbe()
      {
        //The server's ends of the pipes are inherited, ours are not
        SECURITY_ATTRIBUTES inheritable{ .nLength = sizeof(SECURITY_ATTRIBUTES), .bInheritHandle = TRUE };
        winrt::file_handle serverInput, serverOutput;
        winrt::check_bool(CreatePipe(serverInput.put(), _input.put(), &inheritable, 0));
        winrt::check_bool(CreatePipe(_output.put(), serverOutput.put(), &inheritable, 0));
        winrt::check_bool(SetHandleInformation(_input.get(), HANDLE_FLAG_INHERIT, 0));
        winrt::check_bool(SetHandleInformation(_output.get(), HANDLE_FLAG_INHERIT, 0));
        winrt::file_handle serverError{ CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0, nullptr) };

        STARTUPINFOW startup{ .cb = sizeof(STARTUPINFOW), .dwFlags = STARTF_USESTDHANDLES, .hStdInput = serverInput.get(), .hStdOutput = serverOutput.get(), .hStdError = serverError.get() };
        PROCESS_INFORMATION process{};
        auto commandLine = format(L"\"{}\"", AppEnvironment().McpExecutable().wstring());
        winrt::check_bool(CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process));
        _process.attach(process.hProcess);
        CloseHandle(process.hThread);

        _reader = thread([this] { ReadLines(); });
      }

      ~McpProbe()
      {
        if (_input) Close();
        if (WaitForSingleObject(_process.get(), 0) == WAIT_TIMEOUT) TerminateProcess(_process.get(), 1);
        _reader.join();
      }

      void Send(string_view message)
      {
        auto line = string(message) + "\n";
        DWORD written = 0;
        winrt::check_bool(WriteFile(_input.get(), line.data(), DWORD(line.size()), &written, nullptr));
      }

      value_ptr<json_value> Receive()
      {
        string line;
        Assert::IsTrue(_lines.try_get(line, 10s), L"the server should answer");

        string_view text = line;
        auto json = json_value::from_string(text);
        Assert::IsTrue(bool(json), L"the server should write JSON only");
        return json;
      }

      //Closes stdin, as the client does when it stops the server, and returns the exit code.
      optional<DWORD> Close()
      {
        _input.close();
        if (WaitForSingleObject(_process.get(), 5000) != WAIT_OBJECT_0) return nullopt;

        DWORD exitCode = 0;
        GetExitCodeProcess(_process.get(), &exitCode);
        return exitCode;
      }

    private:
      winrt::handle _process;
      winrt::file_handle _input;
      winrt::file_handle _output;
      blocking_collection<string> _lines;
      thread _reader;

      void ReadLines()
      {
        string buffer;
        char chunk[4096];
        DWORD read = 0;
        while (ReadFile(_output.get(), chunk, sizeof(chunk), &read, nullptr) && read > 0)
        {
          buffer.append(chunk, read);
          for (auto end = buffer.find('\n'); end != string::npos; end = buffer.find('\n'))
          {
            _lines.add(buffer.substr(0, end));
            buffer.erase(0, end + 1);
          }
        }
      }
    };
  }

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
