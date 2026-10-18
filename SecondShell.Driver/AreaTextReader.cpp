#include "pch.h"
#include "AreaTextReader.h"
#include "ShelfPacker.h"

using namespace std;

namespace SecondShell::Capture
{
  //Flat background between the cells of an atlas, wide enough that an engine does not join their words
  const uint32_t _gutter = 20;

  namespace
  {
    bool HasCentreIn(const Rect& rect, const ScreenWord& word)
    {
      auto x = word.Box.X + int32_t(word.Box.Width / 2);
      auto y = word.Box.Y + int32_t(word.Box.Height / 2);
      return x >= rect.X && x < rect.Right() && y >= rect.Y && y < rect.Bottom();
    }

    bool IsInside(const Rect& outer, const Rect& inner)
    {
      return inner.Intersection(outer) == inner;
    }
  }

  AreaTextReader::AreaTextReader(shared_ptr<ITextRecognizer> recognizer) :
    _recognizer(move(recognizer))
  { }

  vector<vector<ScreenWord>> AreaTextReader::Read(const shared_ptr<const ScreenFrame>& frame, span<const Rect> areas, const ScreenChanges& changes, stop_token stop)
  {
    //What is known, and what is to be read
    vector<vector<ScreenWord>> result(areas.size());
    vector<Plan> plans;
    for (size_t index = 0; index < areas.size(); index++)
    {
      auto fingerprint = frame->CpuPixels().Fingerprint(areas[index]);
      auto known = _texts.Find(areas[index]);
      if (known && known->Fingerprint == fingerprint)
      {
        result[index] = move(known->Words);
      }
      else
      {
        plans.push_back(PlanReading(index, areas[index], fingerprint, known, changes));
      }
    }

    vector<Rect> rectangles;
    vector<pair<Plan*, const LineStrip*>> owners;
    for (auto& plan : plans)
    {
      for (auto& strip : plan.Strips)
      {
        rectangles.push_back(strip.Read);
        owners.push_back({ &plan, &strip });
      }
    }

    auto words = Recognize(frame, rectangles, stop);
    if (!words) return result;

    //A strip's words replace those in the part which changed; around it, they were read whole before
    for (size_t index = 0; index < rectangles.size(); index++)
    {
      auto [plan, strip] = owners[index];
      for (auto& word : (*words)[index])
      {
        if (HasCentreIn(strip->Changed, word)) plan->Words.push_back(move(word));
      }
    }

    for (auto& plan : plans)
    {
      _texts.Store(areas[plan.Area], { plan.Fingerprint, plan.Words });
      result[plan.Area] = move(plan.Words);
    }
    return result;
  }

  AreaTextReader::Plan AreaTextReader::PlanReading(size_t index, const Rect& area, uint64_t fingerprint, const optional<AreaText>& known, const ScreenChanges& changes) const
  {
    //Only the changed lines, if what is known was read from the pixels the changes start from
    Plan plan{ index, fingerprint };
    if (known && changes.Since && IsInside(changes.Since->Bounds(), area) && known->Fingerprint == changes.Since->CpuPixels().Fingerprint(area))
    {
      plan.Strips = ChangedLines::Of(area, changes.Region);
    }

    if (plan.Strips.empty())
    {
      plan.Strips = { { area, area } };
      return plan;
    }

    for (auto& word : known->Words)
    {
      if (ranges::none_of(plan.Strips, [&](const LineStrip& strip) { return HasCentreIn(strip.Changed, word); })) plan.Words.push_back(word);
    }
    return plan;
  }

  optional<vector<vector<ScreenWord>>> AreaTextReader::Recognize(const shared_ptr<const ScreenFrame>& frame, span<const Rect> rectangles, stop_token stop) const
  {
    vector<vector<ScreenWord>> words(rectangles.size());
    for (auto& layout : ShelfPacker::Pack(rectangles, _recognizer->MaxAtlasSide(), _gutter))
    {
      if (stop.stop_requested()) return nullopt;

      auto atlas = make_shared<TextAtlas>(frame, move(layout));
      for (auto& word : _recognizer->RecognizeAsync(atlas).get())
      {
        if (auto located = atlas->Layout().Locate(word.Box)) words[located->Index].push_back({ move(word.Text), located->Box });
      }
    }
    return words;
  }
}
