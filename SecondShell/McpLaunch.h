#pragma once

namespace SecondShell::Integrations
{
  //How AI apps start SecondShell's MCP server: the command and the arguments their configuration holds (Design.md, AI
  //client integration). Unpackaged, the executable itself; packaged, cmd running its App Execution Alias, which cmd
  //finds on PATH, as some tools cannot start an alias directly (it is a reparse point, not a real file).
  struct McpLaunch
  {
    std::string Command;
    std::vector<std::string> Args;

    McpLaunch() = default;

    //The executable, started directly.
    McpLaunch(const std::filesystem::path& executable) :
      Command(winrt::to_string(executable.wstring()))
    { }

    static McpLaunch ThroughCmd()
    {
      McpLaunch launch;
      launch.Command = "cmd";
      launch.Args = { "/c", "SecondShell.Mcp.exe" };
      return launch;
    }

    //Whether a configured command and arguments start the server this way: paths as Windows compares them, whatever
    //the case and the separators; a program on PATH with or without .exe.
    bool Matches(std::string_view command, const std::vector<std::string>& args) const
    {
      if (Normalize(command) != Normalize(Command) || args.size() != Args.size()) return false;
      for (size_t index = 0; index < args.size(); index++)
      {
        if (Normalize(args[index]) != Normalize(Args[index])) return false;
      }
      return true;
    }

    //Whether a configured command and arguments start any SecondShell MCP server, of any build.
    static bool IsAnyServer(std::string_view command, const std::vector<std::string>& args)
    {
      auto isServer = [](std::string_view text) {
        auto name = std::filesystem::path(winrt::to_hstring(text).c_str()).filename().wstring();
        return _wcsicmp(name.c_str(), L"SecondShell.Mcp.exe") == 0 || _wcsicmp(name.c_str(), L"SecondShell.Mcp") == 0;
        };
      return isServer(command) || std::ranges::any_of(args, isServer);
    }

  private:
    static std::wstring Normalize(std::string_view text)
    {
      auto result = std::filesystem::path(winrt::to_hstring(text).c_str()).lexically_normal().wstring();
      std::ranges::transform(result, result.begin(), towlower);
      if (result.find(L'\\') == std::wstring::npos && result.ends_with(L".exe")) result.resize(result.size() - 4);
      return result;
    }
  };
}
