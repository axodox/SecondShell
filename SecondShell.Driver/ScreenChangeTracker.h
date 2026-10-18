#pragma once
#include "DirtyRegion.h"
#include "ScreenCapturer.h"
#include "TileChangeDetector.h"

namespace SecondShell::Capture
{
  //What changed on the screen between the checks of a wait: the dirty regions the capture reports, collected as frames
  //arrive, and for frames which report none (before Windows 11 24H2), the tiles whose pixels differ. Frames arrive only
  //when the screen changes, so a static screen has no changes and costs nothing.
  class ScreenChangeTracker
  {
  public:
    explicit ScreenChangeTracker(ScreenCapturer& capturer);

    bool HasChanges() const;

    //What changed since the last call; the screen is its newest frame, whose tiles are compared if a frame reported no
    //dirty regions.
    DirtyRegion Take(const Pixels& screen);

  private:
    TileChangeDetector _tiles;  //the caller's thread only

    //Guards what the frame pool's thread collects
    mutable std::mutex _mutex;
    DirtyRegion _reported;
    bool _hasUnreportedChanges = false;

    Axodox::Infrastructure::event_subscription _frameArrivedSubscription;  //last, so it ends first

    void OnFrameArrived(ScreenCapturer* sender, const CapturedFrame& frame);
  };
}
