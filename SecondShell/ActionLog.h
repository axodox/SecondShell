#pragma once
#include "Action.h"
#include "ActionResult.h"

namespace SecondShell::Agent
{
  struct ActionLogEntry
  {
    std::chrono::system_clock::time_point Time;
    std::string Action;  //the action as JSON, sensitive text redacted
    Actions::ActionStatus Status = Actions::ActionStatus::Succeeded;
    std::string Message;
  };

  //Every action the agent ran, for the log page and the log file. Text typed with "sensitive" is redacted.
  class ActionLog
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ActionLog" };
    Axodox::Infrastructure::event_owner _events;

  public:
    ActionLog();

    void Add(const Axodox::Infrastructure::value_ptr<Actions::Action>& action, const Actions::ActionResult& result);
    std::vector<ActionLogEntry> Entries() const;

    Axodox::Infrastructure::event_publisher<ActionLog*, const ActionLogEntry&> EntryAdded;

  private:
    mutable std::mutex _mutex;
    std::deque<ActionLogEntry> _entries;
  };
}
