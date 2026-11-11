#include "pch.h"
#include "ClaudeCodeIntegration.h"
#include "JsonFileIntegration.h"

using namespace std;
using namespace std::chrono;
using namespace std::filesystem;

namespace SecondShell::Integrations
{
  //Node starts slowly the first time
  const seconds _cliTimeout{ 60 };

  ClaudeCodeIntegration::ClaudeCodeIntegration(const UserFolders& folders, McpLaunch launch) :
    _configFile(folders.Profile / ".claude.json"),
    _nativeLauncher(folders.Profile / ".local" / "bin" / "claude.exe"),
    _launch(move(launch))
  { }

  string_view ClaudeCodeIntegration::Id() const
  {
    return "claude-code";
  }

  string_view ClaudeCodeIntegration::Name() const
  {
    return "Claude Code";
  }

  string ClaudeCodeIntegration::Where() const
  {
    return "Through its own command line, for all your projects";
  }

  IntegrationState ClaudeCodeIntegration::State() const
  {
    error_code error;
    if (!exists(_nativeLauncher, error) && !_cli.IsAvailable("claude")) return IntegrationState::NotFound;

    try
    {
      auto entry = JsonConfigEditor{ ReadConfigFile(_configFile) }.Entry("mcpServers", _entryName);
      if (!entry) return IntegrationState::NotAdded;
      return RunsLaunch(*entry, _launch) ? IntegrationState::Added : IntegrationState::Outdated;
    }
    catch (const runtime_error&)
    {
      return IntegrationState::Unreadable;
    }
  }

  IntegrationChange ClaudeCodeIntegration::PreviewAdd() const
  {
    auto commands = CliRunner::CommandLine(Tool(), AddArguments());
    if (State() == IntegrationState::Outdated) commands = CliRunner::CommandLine(Tool(), RemoveArguments()) + "\n" + commands;
    return { {}, {}, commands, "New Claude Code sessions use it; restart the ones running." };
  }

  IntegrationChange ClaudeCodeIntegration::PreviewRemove() const
  {
    return { {}, {}, CliRunner::CommandLine(Tool(), RemoveArguments()), "New Claude Code sessions go without it; restart the ones running." };
  }

  void ClaudeCodeIntegration::Add()
  {
    //It refuses a name it has already: an outdated entry goes first
    if (State() == IntegrationState::Outdated) Run(RemoveArguments());
    Run(AddArguments());
  }

  void ClaudeCodeIntegration::Remove()
  {
    Run(RemoveArguments());
  }

  string ClaudeCodeIntegration::Tool() const
  {
    error_code error;
    return exists(_nativeLauncher, error) ? winrt::to_string(_nativeLauncher.wstring()) : "claude";
  }

  vector<string> ClaudeCodeIntegration::AddArguments() const
  {
    vector<string> arguments{ "mcp", "add", "--transport", "stdio", "--scope", "user", _entryName, "--", _launch.Command };
    arguments.insert(arguments.end(), _launch.Args.begin(), _launch.Args.end());
    return arguments;
  }

  vector<string> ClaudeCodeIntegration::RemoveArguments() const
  {
    return { "mcp", "remove", _entryName, "--scope", "user" };
  }

  void ClaudeCodeIntegration::Run(span<const string> arguments) const
  {
    auto result = _cli.Run(Tool(), arguments, _cliTimeout);
    if (!result) throw runtime_error("claude did not finish within a minute.");
    if (result->ExitCode != 0) throw runtime_error(format("claude failed ({}): {}", result->ExitCode, result->Output));
  }
}
