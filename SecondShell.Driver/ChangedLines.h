#pragma once
#include "DirtyRegion.h"

namespace SecondShell::Capture
{
  //A strip of an area to read again: the rows which changed, and around them what is read so that a changed line of
  //text is whole.
  struct LineStrip
  {
    Rect Read;
    Rect Changed;  //its words replace the ones read before
  };

  //Where an area changed, as whole lines of text: each run of changed rows, as wide as the area, grown by about a line
  //above and below; strips which overlap or touch merged.
  class ChangedLines
  {
  public:
    static std::vector<LineStrip> Of(const Rect& area, const DirtyRegion& changes);
  };
}
