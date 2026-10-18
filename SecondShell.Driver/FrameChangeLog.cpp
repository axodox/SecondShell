#include "pch.h"
#include "FrameChangeLog.h"

using namespace std;

namespace SecondShell::Capture
{
  bool FrameChangeLog::IsEmpty() const
  {
    return _frames.empty();
  }

  void FrameChangeLog::Add(uint64_t number, const Rect& screen, span<const Rect> dirtyRegions)
  {
    _frames.push_back({ number, screen, { dirtyRegions.begin(), dirtyRegions.end() } });
  }

  FrameChangeLog::Changes FrameChangeLog::Take(uint64_t lastNumber, const Rect& screen)
  {
    Changes changes{ DirtyRegion{ screen } };
    while (!_frames.empty() && _frames.front().Number <= lastNumber)
    {
      auto& frame = _frames.front();
      if (frame.Screen != screen) changes.Region.AddAll();
      if (frame.DirtyRegions.empty()) changes.HasUnreported = true;
      for (auto& region : frame.DirtyRegions) changes.Region.Add(region);
      _frames.pop_front();
    }
    return changes;
  }
}
