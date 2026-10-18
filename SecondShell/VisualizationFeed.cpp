#include "pch.h"
#include "VisualizationFeed.h"
#include "ControlActions.h"

using namespace SecondShell::Actions;
using namespace std::chrono;

namespace SecondShell::Agent
{
  VisualizationFeed::VisualizationFeed() :
    EventRaised(_events)
  { }

  uint64_t VisualizationFeed::OnActionStarted(const Action& action)
  {
    auto id = _nextId++;
    _events.raise(EventRaised, this, VisualEvent{ id, steady_clock::now(), action });
    return id;
  }

  void VisualizationFeed::OnActionFinished(uint64_t id, const Action& action, const ActionResult& result)
  {
    _events.raise(EventRaised, this, VisualEvent{ id, steady_clock::now(), action, &result });
  }

  void VisualizationFeed::OnInputReleased()
  {
    OnActionStarted(ControlResetAction{});
  }
}
