#include "pch.h"
#include "CodexIntegration.h"
#include "TomlConfigEditor.h"

using namespace std;
using namespace std::filesystem;

namespace SecondShell::Integrations
{
  const char* const _table = "mcp_servers.secondshell";
  const char* const _restartNote = "New Codex sessions use it; restart the ChatGPT app or the Codex extension.";

  namespace
  {
    //The value of a "key = 'value'" line of the table, as written by SetTable or by codex mcp add
    optional<string> ValueOf(string_view table, string_view key)
    {
      for (auto line : views::split(table, '\n'))
      {
        string_view text{ line.begin(), line.end() };
        auto start = text.find_first_not_of(" \t");
        if (start == string_view::npos || !text.substr(start).starts_with(key)) continue;

        auto equals = text.find('=', start + key.size());
        if (equals == string_view::npos || text.substr(start + key.size(), equals - start - key.size()).find_first_not_of(" \t") != string_view::npos) continue;

        auto value = text.substr(text.find_first_not_of(" \t", equals + 1));
        value = value.substr(0, value.find_last_not_of(" \t\r") + 1);
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') return string{ value.substr(1, value.size() - 2) };
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
          string unescaped;
          for (size_t index = 1; index + 1 < value.size(); index++)
          {
            if (value[index] == '\\' && index + 2 < value.size()) index++;
            unescaped += value[index];
          }
          return unescaped;
        }
      }
      return nullopt;
    }
  }

  CodexIntegration::CodexIntegration(const UserFolders& folders, path mcpExecutable) :
    _home(folders.CodexHome.empty() ? folders.Profile / ".codex" : folders.CodexHome),
    _mcpExecutable(move(mcpExecutable))
  { }

  string_view CodexIntegration::Id() const
  {
    return "codex";
  }

  string_view CodexIntegration::Name() const
  {
    return "Codex";
  }

  string CodexIntegration::Where() const
  {
    return "Its config.toml, for the CLI, the IDE extension and the ChatGPT app";
  }

  IntegrationState CodexIntegration::State() const
  {
    error_code error;
    if (!exists(_home, error)) return IntegrationState::NotFound;

    optional<string> table;
    try
    {
      table = TomlConfigEditor{ ReadConfigFile(ConfigFile()) }.Table(_table);
    }
    catch (const runtime_error&)
    {
      return IntegrationState::Unreadable;
    }
    if (!table) return IntegrationState::NotAdded;

    auto command = ValueOf(*table, "command");
    auto isCurrent = command && _wcsicmp(path{ winrt::to_hstring(*command).c_str() }.lexically_normal().c_str(), _mcpExecutable.lexically_normal().c_str()) == 0;
    return isCurrent ? IntegrationState::Added : IntegrationState::Outdated;
  }

  IntegrationChange CodexIntegration::PreviewAdd() const
  {
    auto before = ReadConfigFile(ConfigFile());
    TomlConfigEditor editor{ before };
    editor.SetTable(_table, { { "command", TomlConfigEditor::Quote(winrt::to_string(_mcpExecutable.wstring())) }, { "args", "[]" } });
    return { ConfigFile(), before, editor.Text(), _restartNote };
  }

  IntegrationChange CodexIntegration::PreviewRemove() const
  {
    auto before = ReadConfigFile(ConfigFile());
    TomlConfigEditor editor{ before };
    editor.RemoveTable(_table);
    return { ConfigFile(), before, editor.Text(), _restartNote };
  }

  void CodexIntegration::Add()
  {
    WriteConfigFile(ConfigFile(), PreviewAdd().After);
  }

  void CodexIntegration::Remove()
  {
    auto change = PreviewRemove();
    if (change.After != change.Before) WriteConfigFile(ConfigFile(), change.After);
  }

  path CodexIntegration::ConfigFile() const
  {
    return _home / "config.toml";
  }
}
