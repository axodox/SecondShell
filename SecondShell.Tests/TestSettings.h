#pragma once
#include "SettingsStore.h"

namespace SecondShell::Tests
{
  //The default settings, from a file that does not exist, so the user's own settings do not change the tests.
  inline void UseDefaultSettings(Axodox::Infrastructure::dependency_container& container)
  {
    auto file = std::filesystem::temp_directory_path() / std::format("SecondShell.Tests.{}.settings.json", GetCurrentProcessId());
    std::filesystem::remove(file);
    container.add<Session::SettingsStore>(std::make_shared<Session::SettingsStore>(file));
  }
}
