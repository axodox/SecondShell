#include "pch.h"
#include "AgentControl.h"

namespace SecondShell::Agent
{
  AgentControl::AgentControl() :
    Stopped(_events)
  { }

  bool AgentControl::IsAgentAllowed() const
  {
    return _isAgentAllowed;
  }

  void AgentControl::Stop()
  {
    _isAgentAllowed = false;
    _events.raise(Stopped, this);
  }

  void AgentControl::GiveBackControl()
  {
    _isAgentAllowed = true;
  }
}
