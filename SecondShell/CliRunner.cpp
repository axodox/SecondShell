#include "pch.h"
#include "CliRunner.h"

using namespace std;
using namespace std::chrono;

namespace SecondShell::Integrations
{
  namespace
  {
    //Quoted for cmd.exe and the C runtime: in double quotes if it has spaces or special characters
    string Quote(string_view argument)
    {
      if (!argument.empty() && argument.find_first_of(" \t\"&|<>^()%!") == string_view::npos) return string{ argument };

      string result = "\"";
      for (auto character : argument)
      {
        if (character == '"') result += '\\';
        result += character;
      }
      return result + "\"";
    }
  }

  string CliRunner::CommandLine(string_view tool, span<const string> arguments)
  {
    auto line = Quote(tool);
    for (auto& argument : arguments) line += " " + Quote(argument);
    return line;
  }

  bool CliRunner::IsAvailable(string_view tool) const
  {
    auto name = winrt::to_hstring(tool);
    for (auto extension : { L".exe", L".cmd" })
    {
      wchar_t found[MAX_PATH];
      if (SearchPathW(nullptr, name.c_str(), extension, MAX_PATH, found, nullptr)) return true;
    }
    return false;
  }

  optional<CliResult> CliRunner::Run(string_view tool, span<const string> arguments, milliseconds timeout) const
  {
    SECURITY_ATTRIBUTES inheritable{ .nLength = sizeof(SECURITY_ATTRIBUTES), .bInheritHandle = TRUE };
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    winrt::check_bool(CreatePipe(&readEnd, &writeEnd, &inheritable, 0));
    winrt::handle reader{ readEnd }, writer{ writeEnd };
    SetHandleInformation(reader.get(), HANDLE_FLAG_INHERIT, 0);

    //cmd /s /c "<line>": the outer quotes go, the line stays as it is
    auto commandLine = winrt::to_hstring(format("cmd.exe /d /s /c \"{}\"", CommandLine(tool, arguments)));
    wstring mutableLine{ commandLine };

    STARTUPINFOW startup{ .cb = sizeof(STARTUPINFOW), .dwFlags = STARTF_USESTDHANDLES, .hStdInput = nullptr, .hStdOutput = writer.get(), .hStdError = writer.get() };
    PROCESS_INFORMATION process{};
    winrt::check_bool(CreateProcessW(nullptr, mutableLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process));
    winrt::handle processHandle{ process.hProcess };
    winrt::handle threadHandle{ process.hThread };
    writer.close();

    //In a job, so a timeout ends the tool's own processes too, which would otherwise keep the pipe open
    winrt::handle job{ CreateJobObjectW(nullptr, nullptr) };
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    AssignProcessToJobObject(job.get(), processHandle.get());
    ResumeThread(threadHandle.get());

    //Read on a thread of its own, so a tool writing much cannot block on a full pipe
    string output;
    thread readThread([&] {
      char buffer[4096];
      DWORD read = 0;
      while (ReadFile(reader.get(), buffer, sizeof(buffer), &read, nullptr) && read > 0) output.append(buffer, read);
      });

    //Once cmd.exe is done, the tool is; anything left over would only keep the pipe open
    auto isDone = WaitForSingleObject(processHandle.get(), DWORD(timeout.count())) == WAIT_OBJECT_0;
    TerminateJobObject(job.get(), 1);
    readThread.join();
    if (!isDone) return nullopt;

    DWORD exitCode = 0;
    GetExitCodeProcess(processHandle.get(), &exitCode);
    return CliResult{ exitCode, move(output) };
  }
}
