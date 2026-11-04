#include "pch.h"
#include "ShellCommandBuilder.h"

using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Desktop
{
  namespace
  {
    //Progress records would reach the error stream; the output is read as UTF-8 (without a BOM). On a line of its own,
    //so error messages, which quote the line, show only the command.
    const char* const _powerShellPrelude = "$ProgressPreference = 'SilentlyContinue'; [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)\n";

    filesystem::path SystemFolder()
    {
      wchar_t folder[MAX_PATH];
      winrt::check_bool(GetSystemDirectoryW(folder, MAX_PATH));
      return folder;
    }

    //A PowerShell string literal: single quotes, in which only a single quote needs escaping (by doubling it)
    string PowerShellLiteral(string_view text)
    {
      string result = "'";
      for (auto character : text)
      {
        if (character == '\'') result += '\'';
        result += character;
      }
      return result + "'";
    }

    wstring JoinArguments(span<const string> args)
    {
      wstring result;
      for (auto& arg : args) result += L" " + ShellCommandBuilder::QuoteArgument(winrt::to_hstring(arg));
      return result;
    }
  }

  ShellCommandBuilder::ShellCommandBuilder() :
    ShellCommandBuilder(SystemFolder())
  { }

  ShellCommandBuilder::ShellCommandBuilder(filesystem::path systemFolder) :
    _systemFolder(move(systemFolder))
  { }

  wstring ShellCommandBuilder::Build(ShellKind shell, string_view command, span<const string> args) const
  {
    //An executable with arguments runs as itself: Windows PowerShell drops the double quotes inside arguments it passes
    //to a program, and cmd has rules of its own; scripts, cmdlets and batch files still go through the shell
    if (!args.empty() && shell != ShellKind::None && IsExecutable(command)) return Executable(command, args);

    switch (shell)
    {
    case ShellKind::PowerShell:
      return PowerShell((_systemFolder / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe").wstring(), command, args);
    case ShellKind::Pwsh:
      return PowerShell(L"pwsh.exe", command, args);
    case ShellKind::Cmd:
      return Cmd(command, args);
    case ShellKind::None:
      return Executable(command, args);
    default:
      throw logic_error("Unknown shell.");
    }
  }

  bool ShellCommandBuilder::IsExecutable(string_view command)
  {
    //As CreateProcess finds it: the path, or the name on the search path, with .exe if it has no extension
    wchar_t path[MAX_PATH];
    auto length = SearchPathW(nullptr, winrt::to_hstring(command).c_str(), L".exe", MAX_PATH, path, nullptr);
    if (length == 0 || length >= MAX_PATH) return false;

    auto extension = filesystem::path(path).extension().wstring();
    return _wcsicmp(extension.c_str(), L".exe") == 0 || _wcsicmp(extension.c_str(), L".com") == 0;
  }

  wstring ShellCommandBuilder::QuoteArgument(wstring_view argument)
  {
    if (!argument.empty() && argument.find_first_of(L" \t\"&|<>^()") == wstring_view::npos) return wstring(argument);

    //Backslashes are literal, except before a quote: there they escape each other, and an odd one escapes the quote
    wstring result = L"\"";
    size_t backslashes = 0;
    for (auto character : argument)
    {
      if (character == L'\\')
      {
        backslashes++;
        continue;
      }

      if (character == L'"') backslashes = backslashes * 2 + 1;
      result.append(backslashes, L'\\');
      result += character;
      backslashes = 0;
    }

    //Before the closing quote, too
    result.append(backslashes * 2, L'\\');
    return result + L"\"";
  }

  wstring ShellCommandBuilder::PowerShell(wstring_view executable, string_view command, span<const string> args) const
  {
    //One argument, which PowerShell reads as the C runtime does. Not -EncodedCommand: with it, errors are written as
    //CLIXML. With arguments, the command is the program, which the call operator runs whether it is an executable, a
    //script or a cmdlet.
    string script = _powerShellPrelude;
    if (args.empty())
    {
      script += command;
    }
    else
    {
      script += "& " + PowerShellLiteral(command);
      for (auto& arg : args) script += " " + PowerShellLiteral(arg);
    }

    return std::format(L"{} -NoLogo -NoProfile -NonInteractive -Command {}", QuoteArgument(executable), QuoteArgument(winrt::to_hstring(script)));
  }

  wstring ShellCommandBuilder::Cmd(string_view command, span<const string> args) const
  {
    //With /s, cmd removes the outer quotes and runs the rest as it is. With arguments, the command is the program.
    wstring line{ winrt::to_hstring(command) };
    if (!args.empty()) line = QuoteArgument(line) + JoinArguments(args);
    return std::format(L"{} /d /s /c \"{}\"", QuoteArgument((_systemFolder / L"cmd.exe").wstring()), line);
  }

  wstring ShellCommandBuilder::Executable(string_view command, span<const string> args) const
  {
    return QuoteArgument(winrt::to_hstring(command)) + JoinArguments(args);
  }
}
