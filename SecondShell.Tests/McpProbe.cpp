#include "pch.h"
#include "McpProbe.h"
#include "AppEnvironment.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Environment;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Tests
{
  McpProbe::McpProbe()
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

  McpProbe::~McpProbe()
  {
    if (_input) Close();
    if (WaitForSingleObject(_process.get(), 0) == WAIT_TIMEOUT) TerminateProcess(_process.get(), 1);
    _reader.join();
  }

  void McpProbe::Send(string_view message)
  {
    auto line = string(message) + "\n";
    DWORD written = 0;
    winrt::check_bool(WriteFile(_input.get(), line.data(), DWORD(line.size()), &written, nullptr));
  }

  value_ptr<json_value> McpProbe::Receive(milliseconds timeout)
  {
    string line;
    Assert::IsTrue(_lines.try_get(line, timeout), L"the server should answer");

    string_view text = line;
    auto json = json_value::from_string(text);
    Assert::IsTrue(bool(json), L"the server should write JSON only");
    return json;
  }

  void McpProbe::Initialize()
  {
    Send(R"({"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "probe", "version": "1"}}})");
    Receive();
    Send(R"({"jsonrpc": "2.0", "method": "notifications/initialized"})");
  }

  value_ptr<json_value> McpProbe::CallTool(string_view name, string_view arguments, milliseconds timeout)
  {
    auto id = _nextId++;
    Send(format(R"({{"jsonrpc": "2.0", "id": {}, "method": "tools/call", "params": {{"name": "{}", "arguments": {}}}}})", id, name, arguments));

    auto deadline = steady_clock::now() + timeout;
    while (true)
    {
      //A zero timeout would wait forever
      auto message = Receive(max(duration_cast<milliseconds>(deadline - steady_clock::now()), 1ms));
      auto& object = message->as<json_object>();
      if (!object.value.contains("id")) continue;

      Assert::AreEqual(double(id), object.at("id")->as<json_number>().value, L"the response should answer the request");
      Assert::IsFalse(object.value.contains("error"), to_wstring(message->to_string()).c_str());
      return object.at("result");
    }
  }

  optional<DWORD> McpProbe::Close()
  {
    _input.close();
    if (WaitForSingleObject(_process.get(), 5000) != WAIT_OBJECT_0) return nullopt;

    DWORD exitCode = 0;
    GetExitCodeProcess(_process.get(), &exitCode);
    return exitCode;
  }

  void McpProbe::ReadLines()
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
}
