#include "pch.h"
#include "ShellCommandBuilder.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace std;

namespace SecondShell::Tests
{
  namespace
  {
    //The command line as a program started with it reads it
    vector<wstring> Parse(const wstring& commandLine)
    {
      int count = 0;
      auto arguments = CommandLineToArgvW(commandLine.c_str(), &count);
      vector<wstring> result(arguments, arguments + count);
      LocalFree(arguments);
      return result;
    }

    //The script of a PowerShell command line, as PowerShell reads it: the argument after -Command
    string Script(const wstring& commandLine)
    {
      auto arguments = Parse(commandLine);
      Assert::AreEqual(wstring(L"-Command"), arguments.at(arguments.size() - 2), L"the script should be one argument");
      return winrt::to_string(arguments.back());
    }
  }

  TEST_CLASS(ShellCommandBuilderTests)
  {
    ShellCommandBuilder _builder{ L"C:\\Windows\\System32" };

  public:
    TEST_METHOD(ArgumentsSurviveQuoting)
    {
      vector<string> args{ "plain", "with space", "with \"quotes\"", "trailing\\", "back\\slash\\\"quote", "", "C:\\Program Files\\", "a&b|c" };
      auto parsed = Parse(_builder.Build(ShellKind::None, "C:\\Program Files\\App\\app.exe", args));

      Assert::AreEqual(args.size() + 1, parsed.size());
      Assert::AreEqual(wstring(L"C:\\Program Files\\App\\app.exe"), parsed[0]);
      for (size_t index = 0; index < args.size(); index++)
      {
        Assert::AreEqual(winrt::to_hstring(args[index]).c_str(), parsed[index + 1].c_str());
      }
    }

    TEST_METHOD(OnlyArgumentsWhichNeedItAreQuoted)
    {
      Assert::AreEqual(wstring(L"notepad.exe"), ShellCommandBuilder::QuoteArgument(L"notepad.exe"));
      Assert::AreEqual(wstring(L"C:\\dir\\file"), ShellCommandBuilder::QuoteArgument(L"C:\\dir\\file"), L"backslashes are literal outside quotes");
      Assert::AreEqual(wstring(L"\"\""), ShellCommandBuilder::QuoteArgument(L""));
      Assert::AreEqual(wstring(L"\"my dir\\\\\""), ShellCommandBuilder::QuoteArgument(L"my dir\\"), L"a backslash before the closing quote is doubled");
    }

    TEST_METHOD(PowerShellGetsTheScriptAsOneArgument)
    {
      auto commandLine = _builder.Build(ShellKind::PowerShell, "Get-Date", {});
      Assert::IsTrue(commandLine.starts_with(L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -NoLogo -NoProfile -NonInteractive -Command"), commandLine.c_str());

      auto script = Script(commandLine);
      Assert::IsTrue(script.ends_with("\nGet-Date"), L"the command should be on a line of its own");
      Assert::IsTrue(script.find("[Console]::OutputEncoding") != string::npos, L"the output should be UTF-8");
    }

    TEST_METHOD(PowerShellArgumentsAreLiterals)
    {
      vector<string> args{ "it's", "$env:USERNAME" };
      auto script = Script(_builder.Build(ShellKind::Pwsh, "Write-Output \"$args\"", args));
      Assert::IsTrue(script.ends_with("& { Write-Output \"$args\" } 'it''s' '$env:USERNAME'"), to_wstring(script).c_str());
    }

    TEST_METHOD(CmdRunsTheCommandAsItIs)
    {
      vector<string> args{ "a b" };
      Assert::AreEqual(wstring(L"C:\\Windows\\System32\\cmd.exe /d /s /c \"echo %PATH% & dir \"a b\"\""), _builder.Build(ShellKind::Cmd, "echo %PATH% & dir", args));
    }
  };
}
