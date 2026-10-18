#include "pch.h"
#include "OverlayRenderer.h"
#include "CaptureFlashOverlay.h"
#include "CursorOverlay.h"
#include "PenOverlay.h"
#include "TouchOverlay.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  OverlayRenderer::OverlayRenderer(dependency_container* container) :
    _feed(container->resolve<VisualizationFeed>()),
    _eventRaisedSubscription(_feed->EventRaised({ this, &OverlayRenderer::OnEventRaised }))
  {
    //Bottom to top
    _elements.push_back(make_unique<CaptureFlashOverlay>());
    _elements.push_back(make_unique<TouchOverlay>());
    _elements.push_back(make_unique<PenOverlay>());
    _elements.push_back(make_unique<CursorOverlay>());
  }

  bool OverlayRenderer::IsAnimating() const
  {
    lock_guard lock{ _mutex };
    return _isAnimating;
  }

  void OverlayRenderer::Draw(ID2D1DeviceContext* context, const ViewTransform& transform, steady_clock::time_point now)
  {
    lock_guard lock{ _mutex };

    _isAnimating = false;
    for (auto& element : _elements) _isAnimating |= element->Draw(context, transform, now);
  }

  void OverlayRenderer::OnEventRaised(VisualizationFeed* /*sender*/, const VisualEvent& event)
  {
    lock_guard lock{ _mutex };

    for (auto& element : _elements) element->OnEvent(event);
    _isAnimating = true;
  }
}
