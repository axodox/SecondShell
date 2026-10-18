#include "pch.h"
#include "TextLines.h"

using namespace std;

namespace SecondShell::Capture
{
  namespace
  {
    uint32_t MedianHeight(span<const ScreenWord> words)
    {
      vector<uint32_t> heights;
      for (auto& word : words) heights.push_back(word.Box.Height);

      auto middle = heights.begin() + heights.size() / 2;
      nth_element(heights.begin(), middle, heights.end());
      return *middle;
    }

    //Doubled, to stay in whole pixels
    int32_t DoubleCentre(const ScreenWord& word)
    {
      return 2 * word.Box.Y + int32_t(word.Box.Height);
    }

    string Join(vector<const ScreenWord*>& line)
    {
      ranges::sort(line, {}, [](const ScreenWord* word) { return word->Box.X; });

      string text;
      for (auto word : line)
      {
        if (!text.empty()) text += ' ';
        text += word->Text;
      }
      return text;
    }
  }

  vector<string> TextLines::FromWords(span<const ScreenWord> words)
  {
    if (words.empty()) return {};

    vector<const ScreenWord*> byCentre;
    for (auto& word : words) byCentre.push_back(&word);
    ranges::stable_sort(byCentre, {}, [](const ScreenWord* word) { return DoubleCentre(*word); });

    //Within half a height, in doubled centres
    auto tolerance = int32_t(MedianHeight(words));
    vector<vector<const ScreenWord*>> lines;
    int32_t lineCentre = 0;
    for (auto word : byCentre)
    {
      auto centre = DoubleCentre(*word);
      if (lines.empty() || abs(centre - lineCentre) >= tolerance)
      {
        lines.emplace_back();
        lineCentre = centre;
      }
      lines.back().push_back(word);
    }

    vector<string> result;
    for (auto& line : lines) result.push_back(Join(line));
    return result;
  }
}
