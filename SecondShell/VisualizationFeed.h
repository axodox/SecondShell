#pragma once
#include "Action.h"
#include "ActionResult.h"

namespace SecondShell::Agent
{
  //An agent action as the overlay sees it: when it started, and once it has finished, also its result. Valid only
  //while it is being raised.
  struct VisualEvent
  {
    uint64_t Id = 0;  //the same for an action's start and finish
    std::chrono::steady_clock::time_point Time;
    const Actions::Action& Action;
    const Actions::ActionResult* Result = nullptr;  //null when started
  };

  //Tells the overlay what the agent does (Design.md, Visualization).
  class VisualizationFeed
  {
    Axodox::Infrastructure::event_owner _events;

  public:
    VisualizationFeed();

    //The ID to finish the action with.
    uint64_t OnActionStarted(const Actions::Action& action);
    void OnActionFinished(uint64_t id, const Actions::Action& action, const Actions::ActionResult& result);

    //Held input was released, e.g. after a failed sequence: shown as a Control.Reset.
    void OnInputReleased();

    Axodox::Infrastructure::event_publisher<VisualizationFeed*, const VisualEvent&> EventRaised;

  private:
    std::atomic_uint64_t _nextId = 1;
  };
}
