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

  }

  AppEnvironment::AppEnvironment() :
    _isPackaged(has_package_identity()),
    _binaryFolder(ThisModuleFolder()),
    _localAppData(LocalAppDataFolder()),
    //The package turns write virtualization off (Packaging\AppxManifest.xml), so its files are where the portable build's are
    _dataFolder(_localAppData / _productFolderName)
  { }

  bool AppEnvironment::IsPackaged() const
  {
    return _isPackaged;
  }

  path AppEnvironment::HostExecutable() const
  {
    return Executable("SecondShell.exe");
  }

  path AppEnvironment::DriverExecutable() const
  {
    return Executable("SecondShell.Driver.exe");
  }

  path AppEnvironment::McpExecutable() const
  {
    return Executable("SecondShell.Mcp.exe");
  }

  path AppEnvironment::Executable(string_view name) const
  {
    //Packaged, the App Execution Alias: it stays the same across updates, unlike the versioned install folder, and
    //starts the program with the package's identity
    return _isPackaged ? _localAppData / "Microsoft" / "WindowsApps" / name : _binaryFolder / name;
  }

  path AppEnvironment::DataFolder() const
  {
    return _dataFolder;
  }

  path AppEnvironment::CapturesFolder() const
  {
    return _dataFolder / "Captures";
  }

  path AppEnvironment::CaptureFolder(string_view sessionName) const
  {
    return CapturesFolder() / sessionName;
  }

  path AppEnvironment::LogFolder() const
  {
    return _dataFolder / "Logs";
  }
}
