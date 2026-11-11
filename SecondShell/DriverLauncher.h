#pragma once
#include "AppEnvironment.h"
#include "SettingsStore.h"

namespace SecondShell::Session
{
  //Starts the driver in the child session (Design.md, Starting the driver). It runs at every sign-in of the user and
  //exits at once in any session which is not the host's child session; with package identity always through its App
  //Execution Alias, whose path stays across updates:
  //- packaged: the package's startup task, always (the manifest enables it, and it goes with the package), which runs
  //  with the startup apps, up to a minute or more after sign-in;
  //- portable, or packaged with the faster start chosen (Settings, DriverStartupTask off): also a per-user scheduled
  //  task, with --logon, which starts it as the session signs in; the host registers it as it starts and removes it as
  //  it exits, so nothing stays behind;
  //- if neither can be had, a RunOnce entry written before each child session's sign-in.
  class DriverLauncher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "DriverLauncher" };

  public:
    explicit DriverLauncher(Axodox::Infrastructure::dependency_container* container);

    //As the host starts, and when the setting changes: the startup task or the scheduled task, for this build's driver.
    //No elevation needed. False if neither could be had, so RunOnce entries start the driver.
    bool Install();

    //As the host exits: removes the scheduled task; the startup task stays with the package.
    void Uninstall();

    //Uninstall: both.
    void RemoveAll();

    bool IsInstalled() const;

    //Before connecting to a new child session: a RunOnce entry, unless a task starts the driver.
    void Arm();

    //When no logon will consume the entry: the connection failed, or on startup after a crash.
    void Disarm();
    bool IsArmed() const;

    //The scheduled task's name, per user, as several users of a PC may each have SecondShell.
    static std::wstring TaskName();

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
    std::shared_ptr<SettingsStore> _settings;
    Axodox::Infrastructure::event_subscription _settingsChangedSubscription;
    std::atomic<int> _appliedChoice = -1;  //the DriverStartupTask setting Install applied, -1 before it ran

    bool InstallScheduledTask();
    void RemoveScheduledTask();
    bool IsScheduled() const;

    //The package's startup task, declared enabled in the manifest; the user may turn it off in Task Manager.
    bool IsStartupTaskEnabled() const;
  };
}
