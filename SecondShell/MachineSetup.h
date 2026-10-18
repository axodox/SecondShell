#pragma once
#include "MachineConfiguration.h"

namespace SecondShell::Setup
{
  //The machine-wide setup steps (Design.md, Setup and repair), safe to run any number of times. Changing anything needs
  //elevation: SecondShell.exe --setup runs them.
  class MachineSetup
  {
  public:
    explicit MachineSetup(Axodox::Infrastructure::dependency_container* container);

    bool AreChildSessionsEnabled() const;
    void EnableChildSessions();

    //About 60 fps in Remote Desktop sessions instead of 30 (Design.md, Smooth session view).
    bool IsSmoothSessionViewOn() const;
    bool IsSmoothSessionViewOurs() const;
    //Off removes the setting only if SecondShell set it; a value set by someone else stays, either way.
    void SetSmoothSessionView(bool isOn);

  private:
    std::shared_ptr<IMachineConfiguration> _configuration;
  };
}
