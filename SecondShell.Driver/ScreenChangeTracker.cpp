#include "pch.h"
#include "ScreenChangeTracker.h"

using namespace std;
using namespace std::chrono;

namespace SecondShell::Capture
{
  const milliseconds _frameTimeout{ 2000 };

  ScreenChangeTracker::ScreenChangeTracker(ScreenCapturer& capturer) :
    _capturer(capturer),
    _frameArrivedSubscription(capturer.FrameArrived({ this, &ScreenChangeTracker::OnFrameArrived }))
  { }

  bool ScreenChangeTracker::HasChanges() const
  {
    lock_guard lock{ _mutex };
    return !_frames.IsEmpty();
  }

  ScreenReading ScreenChangeTracker::Take()
  {
    auto frame = _capturer.Snapshot(_frameTimeout);

    FrameChangeLog::Changes changes;
    {
      lock_guard lock{ _mutex };
      changes = _frames.Take(frame->Number(), frame->Bounds());
    }

    //Compared at every call, so the tiles always compare consecutive snapshots
    auto changedTiles = _tiles.Compare(frame->CpuPixels());
    if (changes.HasUnreported) changes.Region.Add(changedTiles);

    if (!_previous) changes.Region.AddAll();
    return { frame, { move(changes.Region), exchange(_previous, frame) } };
  }

  void ScreenChangeTracker::OnFrameArrived(ScreenCapturer* /*sender*/, const CapturedFrame& frame)
  {
    lock_guard lock{ _mutex };
    _frames.Add(frame.Number, { 0, 0, frame.Width, frame.Height }, frame.DirtyRegions);
  }
}
