#pragma once
#include "Action.h"
#include "ActionResult.h"
#include "HostProtocol.h"

namespace SecondShell::Agent
{
  //An argument of an action as the user reads it: durations in s or ms, sizes as W × H, text in quotes.
  struct ActionArgument
  {
    std::string Name;
    std::string Value;
    bool IsText = false;
  };

  //An action as the status bar and the log show it: its name, the keys it presses as keycaps, and its arguments.
  struct ActionPresentation
  {
    std::string Name;
    std::vector<std::string> Keys;
    std::vector<ActionArgument> Arguments;
  };

  //A capture a step made, where the log can show it from.
  struct CaptureLogInfo
  {
    uint32_t Width = 0;
    uint32_t Height = 0;
    Actions::ImageFormat Format = Actions::ImageFormat::Png;
    size_t Bytes = 0;
    bool IsInline = false;
    std::string File;
  };

  //A command a step ran, and its output.
  struct ProcessLogInfo
  {
    uint32_t ProcessId = 0;
    std::optional<int32_t> ExitCode;
    std::string Output;
  };

  struct ActionLogEntry
  {
    uint32_t Sequence = 0;
    uint32_t Step = 0;
    std::chrono::system_clock::time_point Time;          //when it started
    std::optional<std::chrono::milliseconds> Duration;   //none while it runs
    std::string Action;                                  //the action as JSON, sensitive text redacted
    std::string Summary;                                 //its name and arguments in short, for the log file
    ActionPresentation Presentation;
    std::optional<Actions::ActionStatus> Status;         //none while it runs
    std::string Message;
    std::optional<CaptureLogInfo> Capture;
    std::optional<ProcessLogInfo> Process;
  };

  struct SequenceLogEntry
  {
    uint32_t Number = 0;
    std::string Description;                             //the agent's, or empty
    uint32_t StepCount = 0;
    std::chrono::system_clock::time_point Time;
    std::optional<std::chrono::milliseconds> Duration;   //none while it runs
    std::optional<Actions::ErrorCode> Error;
    std::vector<ActionLogEntry> Steps;
  };

  //A duration as the user reads it: "200 ms", "1.5 s", "12 s", "1 min 30 s".
  std::string FormatDuration(std::chrono::milliseconds duration);

  //Every sequence the agent ran and its steps, for the status bar, the log flyout and the log file. Text typed with
  //"sensitive" is redacted.
  class ActionLog
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ActionLog" };
    Axodox::Infrastructure::event_owner _events;

  public:
    ActionLog();

    //Sequences run one at a time; the steps are logged in the current one.
    void BeginSequence(uint32_t number, const std::optional<std::string>& description, uint32_t stepCount);
    void Start(const Axodox::Infrastructure::value_ptr<Actions::Action>& action, uint32_t step);
    void Add(const Axodox::Infrastructure::value_ptr<Actions::Action>& action, const Actions::ActionResult& result, const std::optional<std::filesystem::path>& captureFile = {});
    void EndSequence(const Protocol::RunSequenceResponse& response);

    //Oldest first, the running step included.
    std::vector<SequenceLogEntry> Sequences() const;

    //A sequence started or ended; without its steps
    Axodox::Infrastructure::event_publisher<ActionLog*, const SequenceLogEntry&> SequenceChanged;
    Axodox::Infrastructure::event_publisher<ActionLog*, const ActionLogEntry&> ActionStarted;
    Axodox::Infrastructure::event_publisher<ActionLog*, const ActionLogEntry&> EntryAdded;

  private:
    mutable std::mutex _mutex;
    std::deque<SequenceLogEntry> _sequences;
    std::optional<ActionLogEntry> _running;

    ActionLogEntry Describe(const Axodox::Infrastructure::value_ptr<Actions::Action>& action, uint32_t step) const;
    SequenceLogEntry& Current();
  };
}
