#pragma once
#include "AtlasLayout.h"

namespace SecondShell::Capture
{
  //Packs rectangles into as few atlases as fit within a maximum side: tallest first, left to right in rows (shelves),
  //with a gutter around each. A rectangle too large for any atlas gets one of its own.
  class ShelfPacker
  {
  public:
    static std::vector<AtlasLayout> Pack(std::span<const Rect> rectangles, uint32_t maxSide, uint32_t gutter);
  };
}
