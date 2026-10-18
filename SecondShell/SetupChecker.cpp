#include "pch.h"
#include "SetupChecker.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Setup
{
  SetupChecker::SetupChecker(dependency_container* container) :
    StatusChanged(_events),
    _machineSetup(container->resolve<MachineSetup>()),
    _driverLauncher(container->resolve<DriverLauncher>()),
    _environment(container->resolve<AppEnvironment>())
  { }

  SetupStatus SetupChecker::Check()
  {
    RepairPerUser();

    SetupStatus status{ _machineSetup->AreChildSessionsEnabled(), _machineSetup->IsSmoothSessionViewOn() };
    {
      lock_guard lock{ _mutex };
      auto isChanged = !_status || _status->AreChildSessionsEnabled != status.AreChildSessionsEnabled || _status->IsSmoothSessionViewOn != status.IsSmoothSessionViewOn;
      _status = status;
      if (!isChanged) return status;
    }

    _logger.log(log_severity::information, "Child sessions {}; smooth session view {}.", status.AreChildSessionsEnabled ? "enabled" : "not enabled", status.IsSmoothSessionViewOn ? "on" : "off");
    _events.raise(StatusChanged, this, status);
    return status;
  }

  SetupStatus SetupChecker::Status() const
  {
    lock_guard lock{ _mutex };
    return _status.value_or(SetupStatus{});
  }

  void SetupChecker::RepairPerUser()
  {
    //A RunOnce entry left by a host that crashed would start the driver at the user's next logon
    _driverLauncher->Disarm();

    error_code error;
    filesystem::create_directories(_environment->DataFolder(), error);
    if (error) _logger.log(log_severity::warning, "Could not create the data folder: {}", error.message());
  }
}
