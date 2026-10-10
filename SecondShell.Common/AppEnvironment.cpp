#include "pch.h"
#include "AppEnvironment.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Environment
{
  const char* const _productFolderName = "SecondShell";

  namespace
  {
    path ThisModuleFolder()
    {
      HMODULE module = nullptr;
      winrt::check_bool(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&ThisModuleFolder), &module));

      wstring buffer(MAX_PATH, L'\0');
      while (true)
      {
        auto length = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()));
        if (length == 0) winrt::throw_last_error();
        if (length < buffer.size()) return path(buffer.substr(0, length)).parent_path();
        buffer.resize(buffer.size() * 2);
      }
    }

    path LocalAppDataFolder()
    {
      PWSTR folder = nullptr;
      winrt::check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &folder));
      path result{ folder };
      CoTaskMemFree(folder);
      return result;
    }

    wstring CurrentPackageFamilyName()
    {
      UINT32 length = 0;
      GetCurrentPackageFamilyName(&length, nullptr);

      wstring result(length, L'\0');
      winrt::check_win32(GetCurrentPackageFamilyName(&length, result.data()));
      result.resize(length - 1);
      return result;
    }
  }

  AppEnvironment::AppEnvironment() :
    _isPackaged(has_package_identity()),
    _binaryFolder(ThisModuleFolder()),
    _localAppData(LocalAppDataFolder())
  {
    //A packaged app's new files under %LOCALAPPDATA% are redirected into the package's LocalCache
    _dataFolder = _isPackaged ?
      _localAppData / "Packages" / CurrentPackageFamilyName() / "LocalCache" / "Local" / _productFolderName :
      _localAppData / _productFolderName;
  }

  bool AppEnvironment::IsPackaged() const
  {
    return _isPackaged;
  }

  path AppEnvironment::HostExecutable() const
  {
    return _binaryFolder / "SecondShell.exe";
  }

  path AppEnvironment::DriverExecutable() const
  {
    return _binaryFolder / "SecondShell.Driver.exe";
  }

  path AppEnvironment::McpExecutable() const
  {
    return _isPackaged ?
      _localAppData / "Microsoft" / "WindowsApps" / "SecondShell.Mcp.exe" :
      _binaryFolder / "SecondShell.Mcp.exe";
  }

  path AppEnvironment::DataFolder() const
  {
    return _dataFolder;
  }

  path AppEnvironment::CaptureFolder(string_view sessionName) const
  {
    return _dataFolder / "Captures" / sessionName;
  }

  path AppEnvironment::LogFolder() const
  {
    return _dataFolder / "Logs";
  }
}
