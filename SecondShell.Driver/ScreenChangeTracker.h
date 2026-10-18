#pragma once
#include "FrameChangeLog.h"
#include "ScreenCapturer.h"
#include "ScreenChanges.h"
#include "TileChangeDetector.h"

namespace SecondShell::Capture
{
  //A snapshot of the screen, and what changed since the previous one.
  struct ScreenReading
  {
    std::shared_ptr<const ScreenFrame> Frame;
    ScreenChanges Changes;
  };

  //What changed on the screen between the checks of a wait: the dirty regions the capture reports, collected as frames
  //arrive, and if a frame reports none (before Windows 11 24H2), the tiles whose pixels differ from the previous
  //snapshot's. Frames arrive only when the screen changes, so a static screen has no changes and costs nothing.
  class ScreenChangeTracker
  {
  public:
    explicit ScreenChangeTracker(ScreenCapturer& capturer);

    bool HasChanges() const;

    //A snapshot, and the changes since the previous call's; the first call's are not known.
    ScreenReading Take();

  private:
    ScreenCapturer& _capturer;
    TileChangeDetector _tiles;  //the caller's thread only
    std::shared_ptr<const ScreenFrame> _previous;

    //Guards what the frame pool's thread collects
    mutable std::mutex _mutex;
    FrameChangeLog _frames;

    Axodox::Infrastructure::event_subscription _frameArrivedSubscription;  //last, so it ends first

    void OnFrameArrived(ScreenCapturer* sender, const CapturedFrame& frame);
  };
}
