#include "pch.h"
#include "AtlasLayout.h"

using namespace std;

namespace SecondShell::Capture
{
  //Engines' boxes may reach a pixel or two past the glyphs, e.g. when scaled back
  const int32_t _edgeTolerance = 3;

  Rect AtlasCell::Placed() const
  {
    return { X, Y, Source.Width, Source.Height };
  }

  optional<LocatedBox> AtlasLayout::Locate(const Rect& box) const
  {
    auto centreX = box.X + int32_t(box.Width / 2);
    auto centreY = box.Y + int32_t(box.Height / 2);
    for (auto& cell : Cells)
    {
      auto placed = cell.Placed();
      if (centreX < placed.X || centreX >= placed.Right() || centreY < placed.Y || centreY >= placed.Bottom()) continue;

      Rect tolerated{ placed.X - _edgeTolerance, placed.Y - _edgeTolerance, placed.Width + 2 * _edgeTolerance, placed.Height + 2 * _edgeTolerance };
      if (box.Intersection(tolerated) != box) return nullopt;

      auto inCell = box.Intersection(placed);
      return LocatedBox{ cell.Index, { inCell.X - cell.X + cell.Source.X, inCell.Y - cell.Y + cell.Source.Y, inCell.Width, inCell.Height } };
    }
    return nullopt;
  }
}
