#pragma once
#include "Action.h"
#include "ActionResult.h"

namespace SecondShell::Agent
{
  struct ActionLogEntry
  {
    std::chrono::system_clock::time_point Time;
    std::string Action;   //the action as JSON, sensitive text redacted
    std::string Summary;  //its name and arguments in short, for the status bar
    Actions::ActionStatus Status = Actions::ActionStatus::Succeeded;
    std::string Message;
  };

  //Every action the agent ran, for the status bar, the log page and the log file. Text typed with "sensitive" is
  //redacted.
  class ActionLog
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ActionLog" };
    Axodox::Infrastructure::event_owner _events;

  public:
    ActionLog();

    //An action starts: only reported, its entry is added when it has finished.
    void Start(const Axodox::Infrastructure::value_ptr<Actions::Action>& action);
    void Add(const Axodox::Infrastructure::value_ptr<Actions::Action>& action, const Actions::ActionResult& result);
    std::vector<ActionLogEntry> Entries() const;

    //With the action's summary
    Axodox::Infrastructure::event_publisher<ActionLog*, const std::string&> ActionStarted;
    Axodox::Infrastructure::event_publisher<ActionLog*, const ActionLogEntry&> EntryAdded;

  private:
    mutable std::mutex _mutex;
    std::deque<ActionLogEntry> _entries;
  };
}
