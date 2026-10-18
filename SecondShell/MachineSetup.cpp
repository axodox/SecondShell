#include "pch.h"
#include "MachineSetup.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Setup
{
  //Frames are composed every this many milliseconds: 65 fps measured (Findings.md 3); the default is about 30
  const uint32_t _smoothFrameInterval = 15;

  MachineSetup::MachineSetup(dependency_container* container) :
    _configuration(container->resolve<IMachineConfiguration>())
  { }

  bool MachineSetup::AreChildSessionsEnabled() const
  {
    return _configuration->AreChildSessionsEnabled();
  }

  void MachineSetup::EnableChildSessions()
  {
    if (!_configuration->AreChildSessionsEnabled()) _configuration->EnableChildSessions();
  }

  bool MachineSetup::IsSmoothSessionViewOn() const
  {
    auto interval = _configuration->FrameInterval();
    return interval && *interval <= _smoothFrameInterval;
  }

  void MachineSetup::SetSmoothSessionView(bool isOn)
  {
    if (isOn)
    {
      if (_configuration->FrameInterval()) return;

      _configuration->FrameInterval(_smoothFrameInterval);
      _configuration->IsFrameIntervalOurs(true);
    }
    else if (_configuration->IsFrameIntervalOurs())
    {
      _configuration->FrameInterval(nullopt);
      _configuration->IsFrameIntervalOurs(false);
    }
  }
}
