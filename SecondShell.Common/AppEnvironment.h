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

    //Packaged, their App Execution Aliases, which survive updates.
    std::filesystem::path HostExecutable() const;
    std::filesystem::path DriverExecutable() const;
    std::filesystem::path McpExecutable() const;

    std::filesystem::path DataFolder() const;
    std::filesystem::path CapturesFolder() const;  //holds a folder per run
    std::filesystem::path CaptureFolder(std::string_view sessionName) const;
    std::filesystem::path LogFolder() const;

  private:
    std::filesystem::path Executable(std::string_view name) const;

    bool _isPackaged;
    std::filesystem::path _binaryFolder;
    std::filesystem::path _localAppData;
    std::filesystem::path _dataFolder;
  };
}
