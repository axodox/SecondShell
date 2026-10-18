#include "pch.h"
#include "ProcessActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  ProcessActionHandler::ProcessActionHandler(dependency_container* container) :
    _processes(container->resolve<ProcessManager>())
  { }

  value_ptr<ActionResult> ProcessActionHandler::Execute(const Action& action, const CancellationToken& cancellation)
  {
    try
    {
      return Run(action, cancellation);
    }
    catch (const invalid_argument& exception)
    {
      return ActionResult::Failed(ErrorCode::InvalidArgument, exception.what());
    }
  }

  value_ptr<ActionResult> ProcessActionHandler::Run(const Action& action, const CancellationToken& cancellation)
  {
    auto timeout = action.WaitTimeout().value_or(Action::DefaultWaitTimeout);
    switch (action.Type())
    {
    case ActionType::ProcessRun:
      return RunProcess(static_cast<const ProcessRunAction&>(action), cancellation);
    case ActionType::ProcessStop:
    {
      auto processId = static_cast<const ProcessStopAction&>(action).ProcessId;
      return Exited(_processes->Stop(processId, timeout, cancellation), processId, timeout, cancellation);
    }
    case ActionType::ProcessKill:
      return Kill(static_cast<const ProcessKillAction&>(action));
    case ActionType::ProcessWaitForProcess:
    {
      auto processId = static_cast<const ProcessWaitForProcessAction&>(action).ProcessId;
      return Exited(_processes->Wait(processId, timeout, cancellation), processId, timeout, cancellation);
    }
    case ActionType::ProcessEnumerate:
      return Enumerate();
    default:
      throw logic_error(format("{} is not a process action.", action.Name()));
    }
  }

  value_ptr<ActionResult> ProcessActionHandler::RunProcess(const ProcessRunAction& action, const CancellationToken& cancellation)
  {
    RunOptions options{
      .Shell = action.Shell.value_or(ShellKind::PowerShell),
      .Command = action.Command,
      .Args = action.Args,
      .WorkingDirectory = action.WorkingDirectory
    };

    if (action.Async.value_or(false))
    {
      auto result = make_value<ProcessResult>();
      result->ProcessId = _processes->Launch(options);
      return result;
    }

    auto processId = _processes->Start(options);
    auto timeout = action.WaitTimeout().value_or(Action::DefaultWaitTimeout);
    return Exited(_processes->Wait(processId, timeout, cancellation), processId, timeout, cancellation);
  }

  value_ptr<ActionResult> ProcessActionHandler::Kill(const ProcessKillAction& action)
  {
    _processes->Kill(action.ProcessId, action.Tree.value_or(false));

    auto result = make_value<ProcessResult>();
    result->ProcessId = action.ProcessId;
    return result;
  }

  value_ptr<ActionResult> ProcessActionHandler::Enumerate()
  {
    auto result = make_value<ProcessListResult>();
    result->Processes = _processes->Enumerate();
    return result;
  }

  value_ptr<ActionResult> ProcessActionHandler::Exited(optional<ProcessResult>&& result, uint32_t processId, milliseconds timeout, const CancellationToken& cancellation)
  {
    if (result) return make_value<ProcessResult>(move(*result));
    if (cancellation.IsCancelled()) return ActionResult::Cancelled();

    //The failed step's result is not returned, so the message carries the process ID
    return ActionResult::Failed(ErrorCode::Timeout, format("Process {} did not exit within {} ms; it keeps running, Process.Stop or Process.Kill ends it.", processId, timeout.count()));
  }
}
