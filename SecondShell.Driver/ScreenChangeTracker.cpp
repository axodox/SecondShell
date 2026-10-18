#include "pch.h"
#include "ScreenChangeTracker.h"

using namespace std;

namespace SecondShell::Capture
{
  ScreenChangeTracker::ScreenChangeTracker(ScreenCapturer& capturer) :
    _reported(capturer.Screen()),
    _frameArrivedSubscription(capturer.FrameArrived({ this, &ScreenChangeTracker::OnFrameArrived }))
  { }

  bool ScreenChangeTracker::HasChanges() const
  {
    lock_guard lock{ _mutex };
    return _hasUnreportedChanges || !_reported.IsEmpty();
  }

  DirtyRegion ScreenChangeTracker::Take(const Pixels& screen)
  {
    DirtyRegion changes;
    bool isComparing;
    {
      lock_guard lock{ _mutex };
      changes = exchange(_reported, DirtyRegion{ _reported.Screen() });
      isComparing = exchange(_hasUnreportedChanges, false);
    }

    if (isComparing) changes.Add(_tiles.Compare(screen));
    return changes;
  }

  void ScreenChangeTracker::OnFrameArrived(ScreenCapturer* /*sender*/, const CapturedFrame& frame)
  {
    lock_guard lock{ _mutex };
    Rect screen{ 0, 0, frame.Width, frame.Height };
    if (_reported.Screen() != screen)
    {
      _reported = DirtyRegion{ screen };
      _reported.AddAll();
    }

    if (frame.DirtyRegions.empty()) _hasUnreportedChanges = true;
    for (auto& region : frame.DirtyRegions) _reported.Add(region);
  }
}
