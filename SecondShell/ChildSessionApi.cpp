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

  optional<system_clock::time_point> ChildSessionApi::LogonTime(uint32_t sessionId) const
  {
    WTSINFOW* info = nullptr;
    DWORD size = 0;
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSSessionInfo, reinterpret_cast<LPWSTR*>(&info), &size) || !info) return nullopt;

    auto logonTime = info->LogonTime.QuadPart;
    WTSFreeMemory(info);
    if (!logonTime) return nullopt;

    //FILETIME counts 100 ns from 1601
    return clock_cast<system_clock>(file_clock::time_point{ file_clock::duration{ logonTime } });
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
