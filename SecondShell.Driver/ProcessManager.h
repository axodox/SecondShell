#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "ShellCommandBuilder.h"

namespace SecondShell::Desktop
{
  struct RunOptions
  {
    Actions::ShellKind Shell = Actions::ShellKind::PowerShell;
    std::string Command;
    std::vector<std::string> Args;
    std::optional<std::string> WorkingDirectory;  //default: the user's profile folder
  };

  //Starts, stops and waits for the processes of the session, and keeps the output of the processes it started until
  //they are waited for. Processes of other sessions are refused, so the agent cannot touch the user's own.
  class ProcessManager
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ProcessManager" };

  public:
    explicit ProcessManager(Axodox::Infrastructure::dependency_container* container);

    //Starts a command whose output is collected: without a window, with empty input. Returns the process ID; throws if
    //the process cannot start.
    uint32_t Start(const RunOptions& options);

    //Starts a program as Explorer would, e.g. a GUI application or a console to interact with: console programs get a
    //window of their own, and nothing is redirected. Returns the process ID; throws if the process cannot start.
    uint32_t Launch(const RunOptions& options);

    //Waits for the exit; nullopt on timeout or cancellation. Processes from Start report their output too.
    std::optional<Actions::ProcessResult> Wait(uint32_t processId, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation);

    //Asks the process to exit, as a user would: a close request to its windows, Ctrl+C to its console. Then waits.
    std::optional<Actions::ProcessResult> Stop(uint32_t processId, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation);

    //Terminates the process, with includeDescendants its descendants too, and returns once they have exited.
    void Kill(uint32_t processId, bool includeDescendants);

    std::vector<Actions::ProcessInfo> Enumerate() const;

  private:
    class StartedProcess;

    std::shared_ptr<ShellCommandBuilder> _commands;
    uint32_t _sessionId = 0;

    std::mutex _mutex;
    std::unordered_map<uint32_t, std::shared_ptr<StartedProcess>> _started;

    //A process can be attached to one console at a time
    std::mutex _consoleMutex;

    //Throws invalid_argument for a process which does not exist or is not in this session.
    winrt::handle Open(uint32_t processId, DWORD access) const;
    std::vector<uint32_t> Descendants(uint32_t processId) const;
    void SendCtrlC(uint32_t processId);
  };
}
