#pragma once
#include "DirtyRegion.h"
#include "Pixels.h"

namespace SecondShell::Capture
{
  //Which tiles of the screen changed since the last call, by their fingerprints: the dirty regions where the capture
  //reports none.
  class TileChangeDetector
  {
  public:
    //All of the screen at the first call, and after a resolution change.
    DirtyRegion Compare(const Pixels& screen);

  private:
    Rect _screen;
    std::vector<uint64_t> _fingerprints;
  };
}
