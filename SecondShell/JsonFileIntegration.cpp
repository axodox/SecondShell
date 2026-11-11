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

  string JsonArray(const vector<string>& texts)
  {
    string result = "[";
    for (auto& text : texts) result += (result.size() > 1 ? ", " : "") + JsonString(text);
    return result + "]";
  }

  namespace
  {
    //The entry's "command" and "args" (none: empty)
    optional<pair<string, vector<string>>> CommandOf(string_view entry)
    {
      auto json = json_value::from_string(entry);
      if (!json || json->type() != json_type::object) return nullopt;

      auto& object = json->as<json_object>();
      auto command = object.value.find("command");
      if (command == object.value.end() || !command->second || command->second->type() != json_type::string) return nullopt;

      vector<string> args;
      if (auto list = object.value.find("args"); list != object.value.end() && list->second && list->second->type() == json_type::array)
      {
        for (auto& item : list->second->as<json_array>().value)
        {
          if (item && item->type() == json_type::string) args.push_back(item->as<json_string>().value);
        }
      }
      return pair{ command->second->as<json_string>().value, move(args) };
    }
  }

  bool RunsLaunch(string_view entry, const McpLaunch& launch)
  {
    auto command = CommandOf(entry);
    return command && launch.Matches(command->first, command->second);
  }

  bool RunsAnyMcpServer(string_view entry)
  {
    auto command = CommandOf(entry);
    return command && McpLaunch::IsAnyServer(command->first, command->second);
  }

  JsonFileIntegration::JsonFileIntegration(JsonFileClient client, McpLaunch launch) :
    _client(move(client)),
    _launch(move(launch))
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
      return RunsLaunch(*editor.Entry(_client.Section, *name), _launch) ? IntegrationState::Added : IntegrationState::Outdated;
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

    //An entry that is there keeps its other settings: only the command and its arguments change
    if (auto name = EntryName(editor))
    {
      editor.SetEntryProperty(_client.Section, *name, "command", JsonString(_launch.Command));
      editor.SetEntryProperty(_client.Section, *name, "args", JsonArray(_launch.Args));
    }
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
    entry.Properties.emplace_back("command", JsonString(_launch.Command));
    entry.Properties.emplace_back("args", JsonArray(_launch.Args));
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
