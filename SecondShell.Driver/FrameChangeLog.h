#pragma once
#include "DirtyRegion.h"

namespace SecondShell::Capture
{
  //The dirty regions of the frames which arrived since the last check, by frame number, so that a check takes exactly
  //the changes its snapshot shows; frames arriving meanwhile stay for the next check.
  class FrameChangeLog
  {
  public:
    struct Changes
    {
      DirtyRegion Region;
      bool HasUnreported = false;  //a frame reported no dirty regions, so the region is incomplete
    };

    bool IsEmpty() const;

    //No dirty regions: the frame does not report them.
    void Add(uint64_t number, const Rect& screen, std::span<const Rect> dirtyRegions);

    //The changes of the frames up to the number, on a screen of the size (all of it, for frames of another size).
    Changes Take(uint64_t lastNumber, const Rect& screen);

  private:
    struct Frame
    {
      uint64_t Number;
      Rect Screen;
      std::vector<Rect> DirtyRegions;
    };

    std::deque<Frame> _frames;
  };
}
