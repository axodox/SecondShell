#pragma once
#include "Rect.h"

namespace SecondShell::Capture
{
  //A rectangle of the screen, placed in an atlas.
  struct AtlasCell
  {
    size_t Index;  //in the rectangles packed
    Rect Source;   //on the screen
    int32_t X;     //in the atlas
    int32_t Y;

    Rect Placed() const;
  };

  //A box found in an atlas, back on the screen.
  struct LocatedBox
  {
    size_t Index;  //of the cell's rectangle
    Rect Box;
  };

  //Where the cells of an atlas are, and the way back from the atlas to the screen.
  struct AtlasLayout
  {
    uint32_t Width = 0;
    uint32_t Height = 0;
    std::vector<AtlasCell> Cells;

    //Through the cell holding the box's centre; nothing if the centre is in a gutter or the box reaches over its cell's
    //edge, as it then joins neighbours.
    std::optional<LocatedBox> Locate(const Rect& box) const;
  };
}
