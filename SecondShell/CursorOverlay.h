#pragma once
#include "OverlayElement.h"

namespace SecondShell::Rendering
{
  //The agent's mouse: a ring where its pointer is, following its moves, with the held buttons filled in, a ripple for
  //each click and arrows for the wheel. It fades out when the mouse is idle. The session's own cursor is in the frames.
  class CursorOverlay : public OverlayElement
  {
    using time_point = std::chrono::steady_clock::time_point;

  public:
    virtual void OnEvent(const Agent::VisualEvent& event) override;
    virtual bool Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now) override;

  private:
    //In session pixels
    struct Motion
    {
      D2D1_POINT_2F From;
      D2D1_POINT_2F To;
      time_point Start;
      std::chrono::milliseconds Duration;
    };

    struct Press
    {
      Actions::MouseButton Button;
      time_point Until;  //max while held
    };

    struct Ripple
    {
      D2D1_POINT_2F Position;
      time_point Start;
    };

    struct WheelTurn
    {
      D2D1_POINT_2F Position;
      time_point Start;
      Actions::WheelAxis Axis;
      bool IsForward;
    };

    std::optional<Motion> _motion;  //empty until the agent's pointer position is known
    time_point _lastActivity;
    std::vector<Press> _presses;
    std::vector<Ripple> _ripples;
    std::vector<WheelTurn> _wheelTurns;

    std::optional<D2D1_POINT_2F> Position(time_point now) const;
    void MoveTo(std::optional<int32_t> x, std::optional<int32_t> y, time_point start, std::chrono::milliseconds duration = {});
    void Click(Actions::MouseButton button, time_point start, std::chrono::milliseconds duration, uint32_t count);
    void Turn(Actions::WheelAxis axis, float delta, time_point start);
    void Release(Actions::MouseButton button);

    float Visibility(time_point now) const;
    void DrawPointer(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, D2D1_POINT_2F center, float radius) const;
    void DrawRipples(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, const ViewTransform& transform, time_point now) const;
    void DrawWheelTurns(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, const ViewTransform& transform, time_point now) const;
  };
}
