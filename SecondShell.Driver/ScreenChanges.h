#pragma once
#include "DirtyRegion.h"
#include "ScreenFrame.h"

namespace SecondShell::Capture
{
  //What changed on the screen since an earlier frame: every change between the two is in the region.
  struct ScreenChanges
  {
    DirtyRegion Region;
    std::shared_ptr<const ScreenFrame> Since;  //null if not known, so anything may have changed
  };
}
