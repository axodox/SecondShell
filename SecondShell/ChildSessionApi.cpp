#include "pch.h"
#include "ChildSessionApi.h"

using namespace std;
using namespace std::chrono;

namespace SecondShell::Session
{
  const milliseconds _logOffPollInterval{ 250 };

  bool ChildSessionApi::IsEnabled() const
  {
    BOOL isEnabled = FALSE;
    return WTSIsChildSessionsEnabled(&isEnabled) && isEnabled;
  }

  optional<uint32_t> ChildSessionApi::ChildSessionId() const
  {
    ULONG sessionId = 0;
    if (!WTSGetChildSessionId(&sessionId) || sessionId == 0 || sessionId == ULONG(-1)) return nullopt;
    return uint32_t(sessionId);
  }

  bool ChildSessionApi::LogOff(uint32_t sessionId, milliseconds timeout) const
  {
    WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, sessionId, FALSE);

    auto deadline = steady_clock::now() + timeout;
    while (ChildSessionId() == sessionId)
    {
      if (steady_clock::now() > deadline) return false;
      this_thread::sleep_for(_logOffPollInterval);
    }
    return true;
  }
}
