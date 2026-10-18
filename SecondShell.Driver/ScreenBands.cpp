#include "pch.h"
#include "ScreenBands.h"

using namespace std;

namespace SecondShell::Capture
{
  const uint32_t _bandHeight = 300;
  const uint32_t _bandOverlap = 48;

  namespace
  {
    uint64_t Area(const Rect& rect)
    {
      return uint64_t(rect.Width) * rect.Height;
    }

    //Overlapping by at least half of the smaller one
    bool IsSamePlace(const Rect& a, const Rect& b)
    {
      auto smaller = min(Area(a), Area(b));
      return smaller > 0 && 2 * Area(a.Intersection(b)) >= smaller;
    }
  }

  vector<Rect> ScreenBands::Layout(const Rect& screen)
  {
    auto height = min(_bandHeight, screen.Height);
    auto lastTop = screen.Bottom() - int32_t(height);

    //The last band ends at the screen's bottom, so it is as tall as the others
    vector<Rect> bands;
    for (auto top = screen.Y; ; top += int32_t(_bandHeight - _bandOverlap))
    {
      top = min(top, lastTop);
      bands.push_back({ screen.X, top, screen.Width, height });
      if (top == lastTop) break;
    }
    return bands;
  }

  vector<ScreenWord> ScreenBands::Merge(vector<ScreenWord> words)
  {
    ranges::stable_sort(words, greater{}, [](const ScreenWord& word) { return Area(word.Box); });

    vector<ScreenWord> result;
    for (auto& word : words)
    {
      if (ranges::none_of(result, [&](const ScreenWord& kept) { return IsSamePlace(kept.Box, word.Box); })) result.push_back(move(word));
    }
    return result;
  }
}
