#pragma once
#include "OverlayElement.h"

namespace SecondShell::Rendering
{
  //The agent's pen: a ring where it hovers, a dot where it touches the screen, and a short fading trail behind it while
  //it draws. It fades out when the pen is idle.
  class PenOverlay : public OverlayElement
  {
    using time_point = std::chrono::steady_clock::time_point;

  public:
    virtual void OnEvent(const Agent::VisualEvent& event) override;
    virtual bool Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now) override;

  private:
    struct TrailPoint
    {
      D2D1_POINT_2F Position;
      time_point Time;
      bool StartsStroke;
    };

    std::optional<Motion> _path;  //empty while out of range
    time_point _contactUntil;     //max while held, past when lifted
    time_point _lastActivity;
    std::vector<TrailPoint> _trail;
    bool _isDrawing = false;  //the trail's last point is of the stroke going on

    void MoveTo(int32_t x, int32_t y, time_point start, std::chrono::milliseconds duration = {});
    bool IsInContact(time_point now) const;
    float Visibility(time_point now) const;
    void DrawTrail(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, const ViewTransform& transform, time_point now) const;
  };
}
