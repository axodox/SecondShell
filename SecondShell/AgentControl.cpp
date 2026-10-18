#include "pch.h"
#include "AgentControl.h"

namespace SecondShell::Agent
{
  AgentControl::AgentControl() :
    ModeChanged(_events)
  { }

  ControlMode AgentControl::Mode() const
  {
    return _mode;
  }

  bool AgentControl::IsAgentAllowed() const
  {
    return _mode == ControlMode::Agent;
  }

  void AgentControl::Stop()
  {
    SetMode(ControlMode::Stopped);
  }

  void AgentControl::TakeControl()
  {
    SetMode(ControlMode::User);
  }

  void AgentControl::GiveBackControl()
  {
    SetMode(ControlMode::Agent);
  }

  void AgentControl::SetMode(ControlMode mode)
  {
    if (_mode.exchange(mode) == mode) return;
    _logger.log(Axodox::Infrastructure::log_severity::information, "Control: {}.", Axodox::Infrastructure::to_string(mode));
    _events.raise(ModeChanged, this, mode);
  }
}
