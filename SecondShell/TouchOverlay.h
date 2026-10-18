#pragma once
#include "OverlayElement.h"

namespace SecondShell::Rendering
{
  //The agent's fingers: a disc for each one down, following its moves, which expands and fades out when it is lifted.
  class TouchOverlay : public OverlayElement
  {
    using time_point = std::chrono::steady_clock::time_point;

  public:
    virtual void OnEvent(const Agent::VisualEvent& event) override;
    virtual bool Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now) override;

  private:
    struct Finger
    {
      uint32_t Id;
      Motion Path;
      time_point Until;  //max while down
    };

    struct Lift
    {
      D2D1_POINT_2F Position;
      time_point Start;
    };

    std::vector<Finger> _fingers;
    std::vector<Lift> _lifts;

    void Press(uint32_t id, int32_t x, int32_t y, time_point start, time_point until = time_point::max());
    void Move(uint32_t id, int32_t x, int32_t y, time_point start, std::chrono::milliseconds duration);
    void Release(uint32_t id, time_point when);
    void ReleaseAll(time_point when);
  };
}
