#pragma once
#include "HostProtocol.h"

namespace SecondShell::Session
{
  //A child session from before, e.g. after a crash of the host, as the user is asked about it.
  struct ExistingSession
  {
    uint32_t SessionId = 0;
    std::optional<std::chrono::system_clock::time_point> LogonTime;
  };

  //Asks the user what to do with a child session from before when Initialize-Session does not say (Design.md, Session
  //lifecycle). The window answers through its handler; without a window nobody is asked.
  class ExistingSessionPrompt
  {
  public:
    //Empty: the user cancelled.
    using Handler = std::function<std::optional<Protocol::ExistingSessionPolicy>(const ExistingSession& session)>;

    void SetHandler(Handler handler);

    //Blocks until the user answers; empty if they cancelled or there is no window.
    std::optional<Protocol::ExistingSessionPolicy> Ask(const ExistingSession& session) const;

  private:
    mutable std::mutex _mutex;
    Handler _handler;
  };
}
