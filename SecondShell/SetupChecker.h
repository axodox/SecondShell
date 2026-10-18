#pragma once
#include "DriverLauncher.h"
#include "MachineSetup.h"

namespace SecondShell::Setup
{
  struct SetupStatus
  {
    bool AreChildSessionsEnabled = false;
    bool IsSmoothSessionViewOn = false;  //optional: reported, never required

    bool IsReady() const { return AreChildSessionsEnabled; }
  };

  //The checks on every start (Design.md, Setup and repair): fixes per-user problems at once, without elevation, and
  //reports the machine-wide state.
  class SetupChecker
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SetupChecker" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit SetupChecker(Axodox::Infrastructure::dependency_container* container);

    //Checks again, e.g. after the elevated setup ran; raises StatusChanged if anything changed.
    SetupStatus Check();
    SetupStatus Status() const;

    Axodox::Infrastructure::event_publisher<SetupChecker*, const SetupStatus&> StatusChanged;

  private:
    std::shared_ptr<MachineSetup> _machineSetup;
    std::shared_ptr<Session::DriverLauncher> _driverLauncher;
    std::shared_ptr<Environment::AppEnvironment> _environment;

    mutable std::mutex _mutex;
    std::optional<SetupStatus> _status;

    void RepairPerUser();
  };
}
