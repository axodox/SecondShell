#pragma once
#include "ActionEnums.h"

namespace SecondShell::Desktop
{
  //The command line which runs a command with a shell. Arguments are passed literally: quoted for the shell, never
  //interpreted by it.
  class ShellCommandBuilder
  {
  public:
    //The shells are started from the system folder, never from a folder an application could put a copy into.
    ShellCommandBuilder();
    explicit ShellCommandBuilder(std::filesystem::path systemFolder);

    std::wstring Build(Actions::ShellKind shell, std::string_view command, std::span<const std::string> args) const;

    //An argument as the C runtime reads command lines (CommandLineToArgvW), quoted only if it has to be.
    static std::wstring QuoteArgument(std::wstring_view argument);

  private:
    std::filesystem::path _systemFolder;

    std::wstring PowerShell(std::wstring_view executable, std::string_view command, std::span<const std::string> args) const;
    std::wstring Cmd(std::string_view command, std::span<const std::string> args) const;
    std::wstring Executable(std::string_view command, std::span<const std::string> args) const;
  };
}
