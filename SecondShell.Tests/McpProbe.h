#pragma once

namespace SecondShell::Tests
{
  //Runs SecondShell.Mcp.exe and speaks MCP with it over stdio, as an AI client does.
  class McpProbe
  {
  public:
    McpProbe();
    ~McpProbe();

    void Send(std::string_view message);

    //The server's next message; fails the test if none arrives in time.
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Receive(std::chrono::milliseconds timeout = std::chrono::seconds(10));

    //Initializes the MCP session.
    void Initialize();

    //Sends tools/call and returns the result, skipping the notifications sent meanwhile.
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> CallTool(std::string_view name, std::string_view arguments, std::chrono::milliseconds timeout);

    //Closes stdin, as the client does when it stops the server, and returns the exit code.
    std::optional<DWORD> Close();

  private:
    winrt::handle _process;
    winrt::file_handle _input;
    winrt::file_handle _output;
    Axodox::Threading::blocking_collection<std::string> _lines;
    std::thread _reader;
    int _nextId = 1;

    void ReadLines();
  };
}
