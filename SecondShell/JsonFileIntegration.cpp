#include "pch.h"
#include "JsonFileIntegration.h"

using namespace Axodox::Json;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Integrations
{
  string JsonString(string_view text)
  {
    return format("\"{}\"", json_escape_string(text));
  }

  namespace
  {
    optional<path> CommandOf(string_view entry)
    {
      auto json = json_value::from_string(entry);
      if (!json || json->type() != json_type::object) return nullopt;

      auto& object = json->as<json_object>();
      auto command = object.value.find("command");
      if (command == object.value.end() || !command->second || command->second->type() != json_type::string) return nullopt;
      return path{ winrt::to_hstring(command->second->as<json_string>().value).c_str() }.lexically_normal();
    }
  }

  bool RunsExecutable(string_view entry, const path& executable)
  {
    //Paths as Windows compares them: whatever the case, and the separators
    auto command = CommandOf(entry);
    return command && _wcsicmp(command->c_str(), executable.lexically_normal().c_str()) == 0;
  }

  bool RunsAnyMcpServer(string_view entry)
  {
    //A shell finds it without its extension too
    auto command = CommandOf(entry);
    if (!command) return false;

    auto name = command->filename().wstring();
    return _wcsicmp(name.c_str(), L"SecondShell.Mcp.exe") == 0 || _wcsicmp(name.c_str(), L"SecondShell.Mcp") == 0;
  }

  JsonFileIntegration::JsonFileIntegration(JsonFileClient client, path mcpExecutable) :
    _client(move(client)),
    _mcpExecutable(move(mcpExecutable))
  { }

  string_view JsonFileIntegration::Id() const
  {
    return _client.Id;
  }

  string_view JsonFileIntegration::Name() const
  {
    return _client.Name;
  }

  string JsonFileIntegration::Where() const
  {
    return _client.Where;
  }

  IntegrationState JsonFileIntegration::State() const
  {
    try
    {
      auto text = ReadConfigFile(_client.ConfigFile);
      if (text.empty() && !IsInstalled()) return IntegrationState::NotFound;

      JsonConfigEditor editor{ text };
      auto name = EntryName(editor);
      if (!name) return IntegrationState::NotAdded;
      return RunsExecutable(*editor.Entry(_client.Section, *name), _mcpExecutable) ? IntegrationState::Added : IntegrationState::Outdated;
    }
    catch (const runtime_error&)
    {
      return IntegrationState::Unreadable;
    }
  }

  IntegrationChange JsonFileIntegration::PreviewAdd() const
  {
    auto before = ReadConfigFile(_client.ConfigFile);
    JsonConfigEditor editor{ before };

    //An entry that is there keeps its other settings: only the command changes
    if (auto name = EntryName(editor)) editor.SetEntryProperty(_client.Section, *name, "command", JsonString(winrt::to_string(_mcpExecutable.wstring())));
    else editor.SetEntry(_client.Section, _entryName, Entry());
    return { _client.ConfigFile, before, editor.Text(), _client.RestartNote };
  }

  IntegrationChange JsonFileIntegration::PreviewRemove() const
  {
    auto before = ReadConfigFile(_client.ConfigFile);
    JsonConfigEditor editor{ before };
    if (auto name = EntryName(editor)) editor.RemoveEntry(_client.Section, *name);
    return { _client.ConfigFile, before, editor.Text(), _client.RestartNote };
  }

  void JsonFileIntegration::Add()
  {
    WriteConfigFile(_client.ConfigFile, PreviewAdd().After);
  }

  void JsonFileIntegration::Remove()
  {
    auto change = PreviewRemove();
    if (change.After != change.Before) WriteConfigFile(_client.ConfigFile, change.After);
  }

  JsonEntry JsonFileIntegration::Entry() const
  {
    JsonEntry entry;
    if (_client.HasTypeField) entry.Properties.emplace_back("type", JsonString("stdio"));
    entry.Properties.emplace_back("command", JsonString(winrt::to_string(_mcpExecutable.wstring())));
    entry.Properties.emplace_back("args", "[]");
    return entry;
  }

  optional<string> JsonFileIntegration::EntryName(const JsonConfigEditor& editor) const
  {
    if (editor.Entry(_client.Section, _entryName)) return _entryName;

    for (auto& name : editor.EntryNames(_client.Section))
    {
      if (RunsAnyMcpServer(*editor.Entry(_client.Section, name))) return name;
    }
    return nullopt;
  }

  bool JsonFileIntegration::IsInstalled() const
  {
    return ranges::any_of(_client.InstallMarkers, [](const path& marker) { error_code error; return exists(marker, error); });
  }
}
