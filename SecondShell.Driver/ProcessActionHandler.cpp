#include "pch.h"
#include "ProcessActionHandler.h"
#include "AnsiText.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  //Process.ReadOutput: the last lines by default, and at most this much
  const uint32_t _defaultTailLines = 50;
  const size_t _defaultReadBytes = 16384;
  const milliseconds _outputPollInterval{ 250 };
  const uint32_t _failureTailLines = 5;

  ProcessActionHandler::ProcessActionHandler(dependency_container* container) :
    _connection(container->resolve<HostConnection>()),
    _processes(container->resolve<ProcessManager>())
  {
    //The host shows the output live; before the peer starts, as handlers must
    _connection->Peer().AddNotificationHandler(this, &ProcessActionHandler::OnConfigureOutput);
    _processes->OutputChanged = [connection = _connection](const ProcessOutputNotification& notification) {
      if (connection->IsAccepted()) connection->Peer().Notify(notification);
      };
  }

  void ProcessActionHandler::OnConfigureOutput(const ConfigureOutputNotification& notification)
  {
    _processes->SetOutputPartSize(uint64_t(notification.PartSize) * 1024 * 1024);
  }

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
    case ActionType::ProcessReadOutput:
      return ReadOutput(static_cast<const ProcessReadOutputAction&>(action));
    case ActionType::ProcessWaitForOutput:
      return WaitForOutput(static_cast<const ProcessWaitForOutputAction&>(action), cancellation);
    case ActionType::ProcessWriteInput:
    {
      auto& write = static_cast<const ProcessWriteInputAction&>(action);
      _processes->WriteInput(write.ProcessId, write.Text.value_or(""), write.Close.value_or(false));
      return ActionResult::Succeeded();
    }
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

    if (action.Output.value_or(false))
    {
      auto result = make_value<ProcessResult>();
      result->ProcessId = _processes->StartWithOutput(options);
      result->OutputFile = winrt::to_string(_processes->Output(result->ProcessId)->File().wstring());
      return result;
    }

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

  namespace
  {
    value_ptr<ActionResult> OutputResult(uint32_t processId, const ProcessOutput& output, OutputChunk&& chunk)
    {
      auto result = make_value<ProcessOutputResult>();
      result->ProcessId = processId;
      result->Text = Text::StripEscapes(chunk.Text);  //colors are for the user; the files keep them
      result->From = chunk.From;
      result->Next = chunk.Next;
      result->IsRunning = output.IsRunning();
      result->ExitCode = output.ExitCode();
      if (chunk.IsTruncated) result->IsTruncated = true;
      return result;
    }
  }

  value_ptr<ActionResult> ProcessActionHandler::ReadOutput(const ProcessReadOutputAction& action)
  {
    auto output = _processes->Output(action.ProcessId);
    auto maxBytes = action.MaxBytes ? size_t(*action.MaxBytes) : _defaultReadBytes;
    auto chunk = action.From ? output->Read(*action.From, maxBytes) : output->Tail(action.Tail.value_or(_defaultTailLines), maxBytes);
    return OutputResult(action.ProcessId, *output, move(chunk));
  }

  value_ptr<ActionResult> ProcessActionHandler::WaitForOutput(const ProcessWaitForOutputAction& action, const CancellationToken& cancellation)
  {
    auto output = _processes->Output(action.ProcessId);
    auto timeout = action.WaitTimeout().value_or(Action::DefaultWaitTimeout);
    auto deadline = steady_clock::now() + timeout;
    while (true)
    {
      //The size first: output after it wakes the wait below
      auto size = output->Size();
      auto isRunning = output->IsRunning();
      if (auto found = output->Find(action.Text, action.From.value_or(0))) return OutputResult(action.ProcessId, *output, move(*found));

      auto lastLines = Text::StripEscapes(output->Tail(_failureTailLines, _defaultReadBytes).Text);
      if (!isRunning)
      {
        auto exitCode = output->ExitCode();
        return ActionResult::Failed(ErrorCode::ActionFailed, format("Process {} exited{} before its output contained \"{}\". Its last lines:\n{}", action.ProcessId, exitCode ? format(" with code {}", *exitCode) : "", action.Text, lastLines));
      }

      auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());
      if (remaining <= 0ms) return ActionResult::Failed(ErrorCode::Timeout, format("The output of process {} did not contain \"{}\" within {} ms. Its last lines:\n{}", action.ProcessId, action.Text, timeout.count(), lastLines));
      if (cancellation.IsCancelled()) return ActionResult::Cancelled();

      output->WaitForChange(size, min(remaining, _outputPollInterval));
    }
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
