#pragma once
#include "DriverLauncher.h"
#include "MachineSetup.h"
#include "PendingRestart.h"

namespace SecondShell::Setup
{
  struct SetupStatus
  {
    bool AreChildSessionsEnabled = false;
    bool IsSmoothSessionViewOn = false;  //optional: reported, never required
    bool IsSmoothSessionViewOurs = false;  //set by SecondShell, so it can turn it off
    bool IsRestartNeeded = false;  //child sessions were enabled since Windows started: they work after a restart

    bool operator==(const SetupStatus&) const = default;

    bool IsReady() const { return AreChildSessionsEnabled && !IsRestartNeeded; }
  };

  //The checks on every start (Design.md, Setup and repair): fixes per-user problems at once, without elevation, and
  //reports the machine-wide state.
  class SetupChecker
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SetupChecker" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit SetupChecker(Axodox::Infrastructure::dependency_container* container);

    //Checks again, e.g. after the elevated setup ran; raises StatusChanged if anything changed. Child sessions found
    //enabled after they were not need a restart, which is noted until Windows restarts.
    SetupStatus Check();
    SetupStatus Status() const;

    Axodox::Infrastructure::event_publisher<SetupChecker*, const SetupStatus&> StatusChanged;

  private:
    std::shared_ptr<MachineSetup> _machineSetup;
    std::shared_ptr<Session::DriverLauncher> _driverLauncher;
    std::shared_ptr<Environment::AppEnvironment> _environment;
    PendingRestart _pendingRestart;

    mutable std::mutex _mutex;
    std::optional<SetupStatus> _status;

    void RepairPerUser();
  };
}
