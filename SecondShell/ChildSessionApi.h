#pragma once

namespace SecondShell::Session
{
  //The WTS functions for the user's child session.
  class ChildSessionApi
  {
  public:
    bool IsEnabled() const;
    std::optional<uint32_t> ChildSessionId() const;

    //When the session's user signed in; empty if Windows does not tell.
    std::optional<std::chrono::system_clock::time_point> LogonTime(uint32_t sessionId) const;

    //Logs the session off and waits until it is gone; false if it is still there after the timeout.
    bool LogOff(uint32_t sessionId, std::chrono::milliseconds timeout) const;
  };
}
