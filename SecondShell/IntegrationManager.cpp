#include "pch.h"
#include "IntegrationManager.h"
#include "ClaudeCodeIntegration.h"
#include "CodexIntegration.h"
#include "JsonFileIntegration.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Integrations
{
  namespace
  {
    bool Exists(const path& item)
    {
      error_code error;
      return exists(item, error);
    }

    //The MSIX build of Claude Desktop reads its configuration from its virtualized AppData (Asgard's McpInstaller,
    //anthropics/claude-code#26073); the installer build from the real one
    path ClaudeDesktopFolder(const UserFolders& folders)
    {
      auto package = folders.LocalData / "Packages" / "Claude_pzs8sxrjxfjjc";
      return Exists(package) ? package / "LocalCache" / "Roaming" / "Claude" : folders.RoamingData / "Claude";
    }

    //Its home is named in a pointer file; newer installs use .lmstudio, older ones .cache\lm-studio
    path LmStudioHome(const UserFolders& folders)
    {
      auto pointer = folders.Profile / ".lmstudio-home-pointer";
      if (Exists(pointer))
      {
        ifstream file{ pointer };
        string home;
        getline(file, home);
        auto end = home.find_last_not_of(" \t\r\n");
        if (end != string::npos) return path{ winrt::to_hstring(home.substr(0, end + 1)).c_str() };
      }

      auto current = folders.Profile / ".lmstudio";
      auto legacy = folders.Profile / ".cache" / "lm-studio";
      return Exists(current) || !Exists(legacy) ? current : legacy;
    }

    //Windsurf became Devin Desktop (2026): the legacy file if it is there, the new one otherwise
    path WindsurfConfig(const UserFolders& folders)
    {
      auto legacy = folders.Profile / ".codeium" / "windsurf";
      return Exists(legacy) ? legacy / "mcp_config.json" : folders.RoamingData / "devin" / "mcp_config.json";
    }
  }

  IntegrationManager::IntegrationManager(dependency_container* container) :
    _mcpExecutable(container->resolve<AppEnvironment>()->McpExecutable()),
    _integrations(Create(UserFolders::Current(), _mcpExecutable))
  { }

  vector<shared_ptr<AiClientIntegration>> IntegrationManager::Create(const UserFolders& folders, const path& mcpExecutable)
  {
    //Locations as of October 2026 (Design.md, Supported clients)
    auto claudeDesktop = ClaudeDesktopFolder(folders);
    auto lmStudio = LmStudioHome(folders);
    vector<JsonFileClient> clients{
      { "claude-desktop", "Claude Desktop", "Its claude_desktop_config.json", claudeDesktop / "claude_desktop_config.json", "mcpServers", false,
        { claudeDesktop, folders.LocalData / "AnthropicClaude" }, "Quit Claude Desktop from the tray and start it again." },
      { "vscode", "VS Code", "Your user mcp.json", folders.RoamingData / "Code" / "User" / "mcp.json", "servers", true,
        { folders.RoamingData / "Code" / "User" }, "VS Code picks it up by itself." },
      { "vscode-insiders", "VS Code Insiders", "Your user mcp.json", folders.RoamingData / "Code - Insiders" / "User" / "mcp.json", "servers", true,
        { folders.RoamingData / "Code - Insiders" / "User" }, "VS Code Insiders picks it up by itself." },
      { "visual-studio", "Visual Studio", ".mcp.json in your profile", folders.Profile / ".mcp.json", "servers", true,
        { folders.ProgramsX86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe" }, "Visual Studio picks it up by itself; enable its tools once in the chat's tool list." },
      { "cursor", "Cursor", "Its mcp.json in your profile", folders.Profile / ".cursor" / "mcp.json", "mcpServers", true,
        { folders.Profile / ".cursor", folders.LocalData / "Programs" / "cursor" }, "Restart Cursor afterwards." },
      { "windsurf", "Windsurf (Devin Desktop)", "Its mcp_config.json", WindsurfConfig(folders), "mcpServers", false,
        { folders.Profile / ".codeium" / "windsurf", folders.RoamingData / "Devin", folders.RoamingData / "devin" }, "Refresh its MCP servers, or restart it." },
      { "lm-studio", "LM Studio", "Its mcp.json", lmStudio / "mcp.json", "mcpServers", false,
        { lmStudio, folders.LocalData / "Programs" / "LM Studio" }, "LM Studio picks it up by itself." },
      { "gemini-cli", "Gemini CLI", "Its settings.json in your profile", folders.Profile / ".gemini" / "settings.json", "mcpServers", false,
        { folders.Profile / ".gemini" }, "New Gemini CLI sessions use it." }
    };

    vector<shared_ptr<AiClientIntegration>> integrations;
    integrations.push_back(make_shared<ClaudeCodeIntegration>(folders, mcpExecutable));
    for (auto& client : clients) integrations.push_back(make_shared<JsonFileIntegration>(move(client), mcpExecutable));
    integrations.push_back(make_shared<CodexIntegration>(folders, mcpExecutable));
    return integrations;
  }

  const vector<shared_ptr<AiClientIntegration>>& IntegrationManager::Integrations() const
  {
    return _integrations;
  }

  shared_ptr<AiClientIntegration> IntegrationManager::Find(string_view id) const
  {
    auto integration = ranges::find_if(_integrations, [&](auto& integration) { return integration->Id() == id; });
    return integration == _integrations.end() ? nullptr : *integration;
  }

  string IntegrationManager::Snippet() const
  {
    return format(R"({{ "mcpServers": {{ "{}": {{ "command": {} }} }} }})", _entryName, JsonString(winrt::to_string(_mcpExecutable.wstring())));
  }
}
