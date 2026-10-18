#pragma once
#include "ViewTransform.h"
#include "VisualizationFeed.h"

namespace SecondShell::Rendering
{
  //One kind of overlay visual (Design.md, Visualization). OverlayRenderer serializes the calls, so an element needs no
  //lock of its own: events come from the sequence's thread, draws from the render thread.
  class OverlayElement
  {
  public:
    virtual void OnEvent(const Agent::VisualEvent& event) = 0;

    //Draws in view pixels; false once nothing animates any more.
    virtual bool Draw(ID2D1DeviceContext* context, const ViewTransform& transform, std::chrono::steady_clock::time_point now) = 0;

    virtual ~OverlayElement() = default;

  protected:
    //How far an animation is, 0..1.
    static float Progress(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now);

    //Fast at first, then slowing down.
    static float EaseOut(float progress);
  };
}
