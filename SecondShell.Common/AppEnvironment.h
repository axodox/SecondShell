#pragma once
#include "CommonApi.h"

namespace SecondShell::Environment
{
  //Everything that differs between the portable and the MSIX build (Design.md, Build differences in code).
  //All executables are installed next to SecondShell.Common.dll.
  class SECONDSHELL_COMMON_API AppEnvironment
  {
  public:
    AppEnvironment();

    bool IsPackaged() const;

    std::filesystem::path HostExecutable() const;
    std::filesystem::path DriverExecutable() const;
    std::filesystem::path McpExecutable() const;  //the App Execution Alias when packaged, which survives updates

    std::filesystem::path DataFolder() const;     //the real path, also when MSIX redirects the writes of the app
    std::filesystem::path CapturesFolder() const;  //holds a folder per run
    std::filesystem::path CaptureFolder(std::string_view sessionName) const;
    std::filesystem::path LogFolder() const;

  private:
    bool _isPackaged;
    std::filesystem::path _binaryFolder;
    std::filesystem::path _localAppData;
    std::filesystem::path _dataFolder;
  };
}
