#include "pch.h"
#include "ChangedLines.h"

using namespace std;

namespace SecondShell::Capture
{
  //About a line of UI text at 100-150 % scale
  const int32_t _lineMargin = 24;

  namespace
  {
    Rect Grown(const Rect& rows, const Rect& area)
    {
      auto top = max(rows.Y - _lineMargin, area.Y);
      auto bottom = min(rows.Bottom() + _lineMargin, area.Bottom());
      return { area.X, top, area.Width, uint32_t(bottom - top) };
    }

    //Both strips span the area's width
    Rect Union(const Rect& a, const Rect& b)
    {
      auto top = min(a.Y, b.Y);
      auto bottom = max(a.Bottom(), b.Bottom());
      return { a.X, top, a.Width, uint32_t(bottom - top) };
    }
  }

  vector<LineStrip> ChangedLines::Of(const Rect& area, const DirtyRegion& changes)
  {
    //The rows come top to bottom, so only the last strip can meet the next one
    vector<LineStrip> strips;
    for (auto& rows : changes.RowsWithin(area))
    {
      LineStrip strip{ Grown(rows, area), rows };
      if (!strips.empty() && strips.back().Read.Bottom() >= strip.Read.Y)
      {
        strips.back() = { Union(strips.back().Read, strip.Read), Union(strips.back().Changed, strip.Changed) };
      }
      else
      {
        strips.push_back(strip);
      }
    }
    return strips;
  }
}
