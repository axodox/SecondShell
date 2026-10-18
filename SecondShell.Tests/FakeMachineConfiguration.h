#pragma once
#include "MachineConfiguration.h"

namespace SecondShell::Tests
{
  //The machine's settings in memory.
  class FakeMachineConfiguration : public Setup::IMachineConfiguration
  {
  public:
    bool ChildSessions = false;
    std::optional<uint32_t> Interval;
    bool IsOurs = false;
    uint32_t ChildSessionsEnabled = 0;

    virtual bool AreChildSessionsEnabled() const override { return ChildSessions; }
    virtual void EnableChildSessions() override { ChildSessions = true; ChildSessionsEnabled++; }

    virtual std::optional<uint32_t> FrameInterval() const override { return Interval; }
    virtual void FrameInterval(std::optional<uint32_t> value) override { Interval = value; }

    virtual bool IsFrameIntervalOurs() const override { return IsOurs; }
    virtual void IsFrameIntervalOurs(bool value) override { IsOurs = value; }
  };
}
