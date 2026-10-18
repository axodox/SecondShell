#pragma once
#include "OverlayElement.h"

namespace SecondShell::Rendering
{
  //Draws the overlay over the session view (Design.md, Visualization): the elements, in order, fed with the agent's
  //actions as they start and finish.
  class OverlayRenderer
  {
  public:
    explicit OverlayRenderer(Axodox::Infrastructure::dependency_container* container);

    //Whether the overlay changes even with no new frame, so the view has to be drawn again.
    bool IsAnimating() const;

    //On the render thread, after the frame.
    void Draw(ID2D1DeviceContext* context, const ViewTransform& transform, std::chrono::steady_clock::time_point now);

  private:
    //Guards the elements, which events and draws reach from different threads
    mutable std::mutex _mutex;
    std::vector<std::unique_ptr<OverlayElement>> _elements;
    bool _isAnimating = false;

    std::shared_ptr<Agent::VisualizationFeed> _feed;
    Axodox::Infrastructure::event_subscription _eventRaisedSubscription;

    void OnEventRaised(Agent::VisualizationFeed* sender, const Agent::VisualEvent& event);
  };
}
