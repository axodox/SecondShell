#pragma once
#include "ActionEnums.h"

namespace SecondShell::Desktop
{
  //The command line which runs a command with a shell. Without arguments the command is a command line for the shell;
  //with arguments it is the program to run, and the arguments are passed literally, never interpreted by a shell: an
  //executable runs as itself, anything else (a script, a cmdlet, a batch file) through the shell, quoted for it.
  class ShellCommandBuilder
  {
  public:
    //The shells are started from the system folder, never from a folder an application could put a copy into.
    ShellCommandBuilder();
    explicit ShellCommandBuilder(std::filesystem::path systemFolder);

    std::wstring Build(Actions::ShellKind shell, std::string_view command, std::span<const std::string> args) const;

    //An argument as the C runtime reads command lines (CommandLineToArgvW), quoted only if it has to be.
    static std::wstring QuoteArgument(std::wstring_view argument);

    //Whether the command is an executable (.exe, .com) CreateProcess would find, rather than a script, a batch file or
    //a shell command.
    static bool IsExecutable(std::string_view command);

  private:
    std::filesystem::path _systemFolder;

    std::wstring PowerShell(std::wstring_view executable, std::string_view command, std::span<const std::string> args) const;
    std::wstring Cmd(std::string_view command, std::span<const std::string> args) const;
    std::wstring Executable(std::string_view command, std::span<const std::string> args) const;
  };
}
