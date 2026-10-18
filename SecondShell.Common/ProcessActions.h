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
    std::optional<uint32_t> Timeout;

    //Async here means not waiting for the exit: the action returns the process ID at once, so it is not an async action
    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
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
