#include "pch.h"
#include "DriverLauncher.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace std;

namespace SecondShell::Session
{
  const wchar_t* const _runOnceKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
  const wchar_t* const _runOnceValue = L"SecondShellDriver";

  DriverLauncher::DriverLauncher(dependency_container* container) :
    _environment(container->resolve<AppEnvironment>())
  { }

  void DriverLauncher::Arm()
  {
    auto command = format(L"\"{}\"", _environment->DriverExecutable().wstring());
    winrt::check_win32(RegSetKeyValueW(HKEY_CURRENT_USER, _runOnceKey, _runOnceValue, REG_SZ, command.c_str(), DWORD((command.size() + 1) * sizeof(wchar_t))));
    _logger.log(log_severity::information, "Armed the driver's RunOnce entry.");
  }

  void DriverLauncher::Disarm()
  {
    if (RegDeleteKeyValueW(HKEY_CURRENT_USER, _runOnceKey, _runOnceValue) == ERROR_SUCCESS)
    {
      _logger.log(log_severity::information, "Removed the driver's RunOnce entry.");
    }
  }

  bool DriverLauncher::IsArmed() const
  {
    return RegGetValueW(HKEY_CURRENT_USER, _runOnceKey, _runOnceValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
  }
}
