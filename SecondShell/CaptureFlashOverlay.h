#pragma once
#include "OverlayElement.h"

namespace SecondShell::Rendering
{
  //A yellow border around what was captured, fading out.
  class CaptureFlashOverlay : public OverlayElement
  {
  public:
    virtual void OnEvent(const Agent::VisualEvent& event) override;
    virtual bool Draw(ID2D1DeviceContext* context, const ViewTransform& transform, std::chrono::steady_clock::time_point now) override;

  private:
    struct Flash
    {
      std::chrono::steady_clock::time_point Start;
      std::optional<D2D1_RECT_F> Region;  //in session pixels; the whole session if empty
    };

    std::vector<Flash> _flashes;
  };
}
