#pragma once
#include "AppEnvironment.h"
#include "HostProtocol.h"

namespace SecondShell::Session
{
  //The user's settings (docs/UIDesign.md, Settings): defaults for what the agent leaves out.
  struct Settings
  {
    static Axodox::Json::json_object_descriptor<Settings> json_description;

    Protocol::DisplaySettings Display;  //of new sessions
    //Seconds
    uint32_t SignInTimeout = uint32_t(std::chrono::duration_cast<std::chrono::seconds>(Protocol::InitializeSessionRequest::DefaultTimeout).count());
    Actions::ImageFormat CaptureFormat = Actions::ImageFormat::Png;
    uint32_t InlineCaptureLimit = 4;
    uint32_t CaptureRetention = 7;      //days
    Actions::ShellKind DefaultShell = Actions::ShellKind::PowerShell;

    //Keep the user's session unlocked behind the lock screen while the agent's session runs, so the agent's input
    //keeps working (Design.md, Working while the screen is locked). Off unless the user turns it on.
    bool WorkWhileLocked = false;

    //The output of processes the agent starts with output goes to files of at most this many megabytes each
    uint32_t OutputPartSize = 100;
    //Captures and output files together, in megabytes; beyond it the oldest are deleted. 0: no limit
    uint32_t StorageLimit = 1024;

    //Packaged: the driver starts with the package's startup task only (removed with the app, but with the startup apps,
    //so up to a minute or more after sign-in); off, a scheduled task too, while SecondShell runs (seconds). The portable
    //build always uses the scheduled task
    bool DriverStartupTask = true;
  };

  //Persistent user settings, as settings.json in the data folder; missing or unreadable, the defaults.
  class SettingsStore
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SettingsStore" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit SettingsStore(Axodox::Infrastructure::dependency_container* container);
    explicit SettingsStore(std::filesystem::path file);  //tests

    Settings Current() const;

    //Changes and saves them, then raises Changed.
    void Update(const std::function<void(Settings&)>& change);

    Axodox::Infrastructure::event_publisher<SettingsStore*, const Settings&> Changed;

  private:
    std::filesystem::path _file;
    mutable std::mutex _mutex;
    Settings _settings;

    void Load();
  };
}
