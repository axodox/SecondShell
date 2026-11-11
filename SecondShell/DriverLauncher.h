#pragma once
#include "AppEnvironment.h"

namespace SecondShell::Session
{
  //Starts the driver in the child session (Design.md, Starting the driver). While the host runs, a per-user scheduled
  //task starts it at every sign-in of the user, with --logon, as the session signs in rather than after Explorer's
  //startup programs; the driver exits at once in any session which is not the host's child session. The host registers
  //the task as it starts and removes it as it exits, so nothing stays behind (an MSIX uninstall removes only what its
  //manifest declares). If the task cannot be registered, a RunOnce entry written before each child session's sign-in
  //starts the driver instead, later.
  class DriverLauncher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "DriverLauncher" };

  public:
    explicit DriverLauncher(Axodox::Infrastructure::dependency_container* container);

    //Registers the sign-in task for this build's driver, or updates it, e.g. after the build moved. No elevation needed.
    //False if Task Scheduler refused it, e.g. by policy.
    bool Install();

    //Removes the sign-in task (uninstall).
    void Uninstall();
    bool IsInstalled() const;

    //Before connecting to a new child session: a RunOnce entry, unless the sign-in task starts the driver.
    void Arm();

    //When no logon will consume the entry: the connection failed, or on startup after a crash.
    void Disarm();
    bool IsArmed() const;

    //The task's name, per user, as several users of a PC may each have SecondShell.
    static std::wstring TaskName();

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
  };
}
