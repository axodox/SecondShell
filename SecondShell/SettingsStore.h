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
