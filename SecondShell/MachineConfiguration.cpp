#include "pch.h"
#include "MachineConfiguration.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Setup
{
  const wchar_t* const _winStationsKey = L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\WinStations";
  const wchar_t* const _frameIntervalValue = L"DWMFRAMEINTERVAL";

  //SecondShell's own machine-wide notes; HKLM, as the elevated setup may run as another account
  const wchar_t* const _setupKey = L"SOFTWARE\\SecondShell";
  const wchar_t* const _frameIntervalOursValue = L"SetFrameInterval";

  namespace
  {
    optional<uint32_t> ReadDword(const wchar_t* key, const wchar_t* name)
    {
      DWORD value = 0;
      DWORD size = sizeof(value);
      if (RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) return nullopt;
      return value;
    }

    void WriteDword(const wchar_t* key, const wchar_t* name, optional<uint32_t> value)
    {
      if (value)
      {
        DWORD data = *value;
        winrt::check_win32(RegSetKeyValueW(HKEY_LOCAL_MACHINE, key, name, REG_DWORD, &data, sizeof(data)));
        return;
      }

      auto result = RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, key, name);
      if (result != ERROR_FILE_NOT_FOUND) winrt::check_win32(result);
    }
  }

  bool MachineConfiguration::AreChildSessionsEnabled() const
  {
    BOOL isEnabled = FALSE;
    return WTSIsChildSessionsEnabled(&isEnabled) && isEnabled;
  }

  void MachineConfiguration::EnableChildSessions()
  {
    winrt::check_bool(WTSEnableChildSessions(TRUE));
    _logger.log(log_severity::information, "Enabled child sessions.");
  }

  optional<uint32_t> MachineConfiguration::FrameInterval() const
  {
    return ReadDword(_winStationsKey, _frameIntervalValue);
  }

  void MachineConfiguration::FrameInterval(optional<uint32_t> value)
  {
    WriteDword(_winStationsKey, _frameIntervalValue, value);
    if (value) _logger.log(log_severity::information, "Set DWMFRAMEINTERVAL to {}.", *value);
    else _logger.log(log_severity::information, "Removed DWMFRAMEINTERVAL.");
  }

  bool MachineConfiguration::IsFrameIntervalOurs() const
  {
    return ReadDword(_setupKey, _frameIntervalOursValue).value_or(0) != 0;
  }

  void MachineConfiguration::IsFrameIntervalOurs(bool value)
  {
    WriteDword(_setupKey, _frameIntervalOursValue, value ? optional<uint32_t>{ 1 } : nullopt);

    //Its key goes with its last value (deleting a key would take any other values with it)
    HKEY key = nullptr;
    if (value || RegOpenKeyExW(HKEY_LOCAL_MACHINE, _setupKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return;

    DWORD values = 0, subkeys = 0;
    auto isEmpty = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS && values == 0 && subkeys == 0;
    RegCloseKey(key);
    if (isEmpty) RegDeleteKeyW(HKEY_LOCAL_MACHINE, _setupKey);
  }
}
