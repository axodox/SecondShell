#include "pch.h"
#include "PendingRestart.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Setup
{
  PendingRestart::PendingRestart(filesystem::path file) :
    _file(move(file))
  { }

  void PendingRestart::Note(optional<uint32_t> bootId)
  {
    if (!bootId) return;

    error_code error;
    filesystem::create_directories(_file.parent_path(), error);
    ofstream stream{ _file, ios::trunc };
    stream << *bootId;
    if (!stream) _logger.log(log_severity::warning, "Could not note that Windows needs to restart.");
  }

  bool PendingRestart::IsPending(optional<uint32_t> bootId)
  {
    uint32_t noted = 0;
    {
      ifstream stream{ _file };
      if (!stream || !(stream >> noted)) return false;
    }

    if (bootId && noted == *bootId) return true;

    error_code error;
    filesystem::remove(_file, error);
    return false;
  }

  bool RestartWindows()
  {
    //Every user may restart, but the privilege is off until a process turns it on
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
      TOKEN_PRIVILEGES privileges{ .PrivilegeCount = 1 };
      privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
      if (LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid)) AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
      CloseHandle(token);
    }

    if (ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_RECONFIG | SHTDN_REASON_FLAG_PLANNED)) return true;

    logger{ "PendingRestart" }.log(log_severity::error, "Windows did not restart: error {}.", GetLastError());
    return false;
  }
}
