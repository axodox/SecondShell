#pragma once
#include "HostProtocol.h"

namespace SecondShell::Session
{
  //Whether input reaches the child session now: only while the user's own session is unlocked (Findings.md 2.17).
  //SequenceRunner waits out a switch with it; tests fake it.
  class IInputAvailability
  {
  public:
    virtual ~IInputAvailability() = default;

    virtual Protocol::InputAvailability Current() const = 0;
  };
}
