#include "pch.h"
#include "SessionEventLog.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  namespace
  {
    //Local time with its offset, which the agent can relate to its own clock
    string Now()
    {
      auto now = floor<seconds>(system_clock::now());
      try
      {
        return format("{:%FT%T%Ez}", zoned_time{ current_zone(), now });
      }
      catch (const exception&)
      {
        return format("{:%FT%TZ}", now);
      }
    }
  }

  SessionEventLog::SessionEventLog(dependency_container* container) :
    _modeChangedSubscription(container->resolve<AgentControl>()->ModeChanged({ this, &SessionEventLog::OnModeChanged }))
  { }

  uint64_t SessionEventLog::End() const
  {
    lock_guard lock{ _mutex };
    return _start + _events.size();
  }

  vector<SessionEvent> SessionEventLog::ReadFrom(uint64_t& position) const
  {
    lock_guard lock{ _mutex };
    auto first = max(position, _start);
    vector<SessionEvent> result{ _events.begin() + ptrdiff_t(first - _start), _events.end() };
    position = _start + _events.size();
    return result;
  }

  void SessionEventLog::Add(SessionEventKind kind, string message)
  {
    _logger.log(log_severity::information, "{}: {}", to_string(kind), message);

    lock_guard lock{ _mutex };
    _events.push_back({ Now(), kind, move(message) });
    if (_events.size() > _limit)
    {
      _events.pop_front();
      _start++;
    }
  }

  void SessionEventLog::OnInputStatusChanged(const InputStatus& previous, const InputStatus& current)
  {
    if (previous.IsUserPresent != current.IsUserPresent)
    {
      if (current.IsUserPresent) Add(SessionEventKind::UserReturned, "The user is back at the PC.");
      else Add(SessionEventKind::UserLeft, "The user left the PC: their Windows session is locked or not on the PC's screen.");
    }

    if (previous.Input != current.Input)
    {
      switch (current.Input)
      {
      case InputAvailability::Resuming:
        Add(SessionEventKind::InputPaused, "The user locked the PC, which stops all input to the session; input actions wait the few seconds SecondShell takes to keep the session unlocked behind the lock screen.");
        break;
      case InputAvailability::Unavailable:
        Add(SessionEventKind::InputUnavailable, "The user's Windows session is locked: input actions fail with InputUnavailable until it is unlocked. Captures, windows, processes and the clipboard still work.");
        break;
      default:
        Add(SessionEventKind::InputAvailable, "Input reaches the session again.");
        break;
      }
    }
  }

  void SessionEventLog::OnModeChanged(AgentControl* /*sender*/, ControlMode mode)
  {
    switch (mode)
    {
    case ControlMode::User:
      Add(SessionEventKind::ControlTaken, "The user took control of the session: sequences fail with AgentInputDisabled until they give it back. Check-Session with waitForControl waits for that.");
      break;
    case ControlMode::Stopped:
      Add(SessionEventKind::AgentStopped, "The user stopped the agent: sequences fail with AgentInputDisabled until they give control back. Check-Session with waitForControl waits for that.");
      break;
    default:
      Add(SessionEventKind::ControlReturned, "The user gave control back: you may use the session again. What is on the screen may have changed meanwhile.");
      break;
    }
  }

  vector<WindowInfo> NewWindowFilter::Update(vector<WindowInfo> windows)
  {
    unordered_set<uint64_t> current;
    for (auto& window : windows) current.insert(window.Window.Handle);

    vector<WindowInfo> result;
    if (_known)
    {
      for (auto& window : windows)
      {
        if (!_known->contains(window.Window.Handle)) result.push_back(move(window));
      }
    }

    //A window which closed and comes back with the same ID is new again
    _known = move(current);
    return result;
  }

  void NewWindowFilter::Reset()
  {
    _known.reset();
  }
}
