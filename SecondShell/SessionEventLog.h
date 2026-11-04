#pragma once
#include "AgentControl.h"
#include "IInputAvailability.h"

namespace SecondShell::Agent
{
  //What changed for the agent: control, and the user's session (Design.md, Telling the agent what changed). The agent
  //hears about it with its next call: each MCP connection reads the events since its previous one.
  class SessionEventLog
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SessionEventLog" };

  public:
    explicit SessionEventLog(Axodox::Infrastructure::dependency_container* container);

    //The position after the newest event: a reader starting there gets only the events after it.
    uint64_t End() const;

    //The events from a position on, moving the position past them. The oldest events are dropped beyond a limit.
    std::vector<Protocol::SessionEvent> ReadFrom(uint64_t& position) const;

    void Add(Protocol::SessionEventKind kind, std::string message);

    //The events of a change of the user's session: presence first, then input.
    void OnInputStatusChanged(const Session::InputStatus& previous, const Session::InputStatus& current);

  private:
    static constexpr size_t _limit = 100;

    mutable std::mutex _mutex;
    std::deque<Protocol::SessionEvent> _events;
    uint64_t _start = 0;  //the position of _events.front()

    Axodox::Infrastructure::event_subscription _modeChangedSubscription;

    void OnModeChanged(AgentControl* sender, ControlMode mode);
  };

  //Which windows are new since the agent's previous call: all windows of the session, dialogs included, against the
  //ones seen last time. The first look only learns what is there.
  class NewWindowFilter
  {
  public:
    std::vector<Actions::WindowInfo> Update(std::vector<Actions::WindowInfo> windows);
    void Reset();

  private:
    std::optional<std::unordered_set<uint64_t>> _known;
  };
}
