#pragma once
#include "Action.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API ProcessRunAction : public TypedAction<ActionKind::Process, ActionType::ProcessRun>
  {
    static Axodox::Json::json_object_descriptor<ProcessRunAction> json_description;

    std::string Command;
    std::optional<ShellKind> Shell;
    std::vector<std::string> Args;
    std::optional<std::string> WorkingDirectory;
    std::optional<bool> Async;
    std::optional<bool> Output;  //with async: collect the output and take input, without a window
    std::optional<uint32_t> Timeout;

    //Async here means not waiting for the exit: the action returns the process ID at once, so it is not an async action
    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
    virtual Axodox::Infrastructure::expected<> Validate() const override;
  };

  //The output of a process started with output: by default its last lines.
  struct SECONDSHELL_COMMON_API ProcessReadOutputAction : public TypedAction<ActionKind::Process, ActionType::ProcessReadOutput>
  {
    static Axodox::Json::json_object_descriptor<ProcessReadOutputAction> json_description;

    uint32_t ProcessId = 0;
    std::optional<uint32_t> Tail;      //lines
    std::optional<uint64_t> From;      //offset, as next of an earlier read returned
    std::optional<uint32_t> MaxBytes;

    virtual Axodox::Infrastructure::expected<> Validate() const override;
  };

  //Waits until a line of the output of a process started with output contains a text.
  struct SECONDSHELL_COMMON_API ProcessWaitForOutputAction : public TypedAction<ActionKind::Process, ActionType::ProcessWaitForOutput>
  {
    static Axodox::Json::json_object_descriptor<ProcessWaitForOutputAction> json_description;

    uint32_t ProcessId = 0;
    std::string Text;
    std::optional<uint64_t> From;
    std::optional<uint32_t> Timeout;

    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
  };

  //Writes to the input of a process started with output.
  struct SECONDSHELL_COMMON_API ProcessWriteInputAction : public TypedAction<ActionKind::Process, ActionType::ProcessWriteInput>
  {
    static Axodox::Json::json_object_descriptor<ProcessWriteInputAction> json_description;

    uint32_t ProcessId = 0;
    std::optional<std::string> Text;
    std::optional<bool> Close;
  };

  struct SECONDSHELL_COMMON_API ProcessStopAction : public TypedAction<ActionKind::Process, ActionType::ProcessStop>
  {
    static Axodox::Json::json_object_descriptor<ProcessStopAction> json_description;

    uint32_t ProcessId = 0;
    std::optional<uint32_t> Timeout;

    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
  };

  struct SECONDSHELL_COMMON_API ProcessKillAction : public TypedAction<ActionKind::Process, ActionType::ProcessKill>
  {
    static Axodox::Json::json_object_descriptor<ProcessKillAction> json_description;

    uint32_t ProcessId = 0;
    std::optional<bool> Tree;
  };

  struct SECONDSHELL_COMMON_API ProcessWaitForProcessAction : public TypedAction<ActionKind::Process, ActionType::ProcessWaitForProcess>
  {
    static Axodox::Json::json_object_descriptor<ProcessWaitForProcessAction> json_description;

    uint32_t ProcessId = 0;
    std::optional<uint32_t> Timeout;

    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
  };

  struct SECONDSHELL_COMMON_API ProcessEnumerateAction : public TypedAction<ActionKind::Process, ActionType::ProcessEnumerate>
  {
    static Axodox::Json::json_object_descriptor<ProcessEnumerateAction> json_description;
  };
}
