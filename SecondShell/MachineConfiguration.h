#pragma once

namespace SecondShell::Setup
{
  //The machine-wide settings SecondShell needs or offers (Design.md, Setup and repair). Reading needs no elevation;
  //writing does.
  class IMachineConfiguration
  {
  public:
    virtual ~IMachineConfiguration() = default;

    virtual bool AreChildSessionsEnabled() const = 0;
    virtual void EnableChildSessions() = 0;

    //DWMFRAMEINTERVAL of the Remote Desktop sessions; empty if not set.
    virtual std::optional<uint32_t> FrameInterval() const = 0;
    virtual void FrameInterval(std::optional<uint32_t> value) = 0;

    //Whether SecondShell set the frame interval, so it may remove it again.
    virtual bool IsFrameIntervalOurs() const = 0;
    virtual void IsFrameIntervalOurs(bool value) = 0;
  };

  //The settings as Windows keeps them: WTS for child sessions, HKLM for the rest.
  class MachineConfiguration : public IMachineConfiguration
  {
    inline static const Axodox::Infrastructure::logger _logger{ "MachineConfiguration" };

  public:
    virtual bool AreChildSessionsEnabled() const override;
    virtual void EnableChildSessions() override;

    virtual std::optional<uint32_t> FrameInterval() const override;
    virtual void FrameInterval(std::optional<uint32_t> value) override;

    virtual bool IsFrameIntervalOurs() const override;
    virtual void IsFrameIntervalOurs(bool value) override;
  };
}
