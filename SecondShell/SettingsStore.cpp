#include "pch.h"
#include "SettingsStore.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Environment;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Session
{
  //Every property is optional, so a file from an earlier version keeps what it has
  json_object_descriptor<Settings> Settings::json_description = describe_json_object<Settings>({
    { &Settings::Display, "display", { .is_required = false } },
    { &Settings::SignInTimeout, "signInTimeout", { .is_required = false, .schema = { .minimum = 30, .maximum = 3600 } } },
    { &Settings::CaptureFormat, "captureFormat", { .is_required = false } },
    { &Settings::InlineCaptureLimit, "inlineCaptureLimit", { .is_required = false, .schema = { .minimum = 0, .maximum = 100 } } },
    { &Settings::CaptureRetention, "captureRetention", { .is_required = false, .schema = { .minimum = 0, .maximum = 3650 } } },
    { &Settings::DefaultShell, "defaultShell", { .is_required = false } }
    });

  SettingsStore::SettingsStore(dependency_container* container) :
    SettingsStore(container->resolve<AppEnvironment>()->DataFolder() / "settings.json")
  { }

  SettingsStore::SettingsStore(path file) :
    Changed(_events),
    _file(move(file))
  {
    Load();
  }

  Settings SettingsStore::Current() const
  {
    lock_guard lock{ _mutex };
    return _settings;
  }

  void SettingsStore::Update(const function<void(Settings&)>& change)
  {
    Settings settings;
    {
      lock_guard lock{ _mutex };
      change(_settings);
      settings = _settings;

      //Written next to it, then moved over it, so a failed write leaves the old file
      error_code error;
      create_directories(_file.parent_path(), error);
      auto temporary = path{ _file }.replace_extension(".json.tmp");
      {
        ofstream file{ temporary, ios::binary | ios::trunc };
        file << json_serializer<Settings>::to_json(settings)->to_string();
      }
      rename(temporary, _file, error);
      if (error) _logger.log(log_severity::error, "Could not save the settings to {}: {}", _file.string(), error.message());
    }

    _events.raise(Changed, this, settings);
  }

  void SettingsStore::Load()
  {
    ifstream file{ _file, ios::binary };
    if (!file) return;

    string text{ istreambuf_iterator<char>(file), istreambuf_iterator<char>() };
    string_view view{ text };
    auto json = json_value::from_string(view);

    //Invalid settings are left in the file until the user changes one, and the defaults used meanwhile
    Settings settings;
    expected<> parsing = format_unexpected("not JSON");
    if (json)
    {
      parsing = Settings::json_description.validate(*json);
      if (parsing) parsing = Settings::json_description.from_json(settings, json.get());
    }

    if (!parsing)
    {
      _logger.log(log_severity::warning, "The settings in {} are not valid ({}); using the defaults.", _file.string(), parsing.error());
      return;
    }

    _settings = settings;
  }
}
