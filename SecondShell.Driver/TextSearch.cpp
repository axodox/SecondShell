#include "pch.h"
#include "TextSearch.h"
#include "TextLines.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Capture
{
  const milliseconds _thoroughPassInterval{ 1500 };

  namespace
  {
    Rect VisibleArea(const WindowInfo& window, const Rect& screen)
    {
      if (window.State == WindowState::Minimized) return {};
      return Rect{ window.X, window.Y, window.Width, window.Height }.Intersection(screen);
    }

    string Text(span<const ScreenWord> words)
    {
      string text;
      for (auto& line : TextLines::FromWords(words)) text += line + '\n';
      return text;
    }
  }

  TextSearch::TextSearch(string_view phrase, ScreenCapturer& capturer, const WindowManager& windows, QuickTextReader& quick, ThoroughTextReader& thorough) :
    _phrase(phrase),
    _windows(windows),
    _quick(quick),
    _thorough(thorough),
    _changes(capturer)
  { }

  void TextSearch::Start()
  {
    //The first reading's changes are not known, so everything is searched
    auto reading = _changes.Take();
    _frame = reading.Frame;
    _changedSinceThoroughPass = reading.Changes;

    SearchWindows(reading.Changes);
    if (!_isFound) StartThoroughPass();
  }

  void TextSearch::Check()
  {
    CollectThoroughPass();
    if (!_isFound) SearchChanges();
    if (!_isFound && IsThoroughPassDue()) StartThoroughPass();
  }

  bool TextSearch::IsFound() const
  {
    return _isFound;
  }

  const string& TextSearch::Shown() const
  {
    return _shown;
  }

  void TextSearch::SearchChanges()
  {
    if (!_changes.HasChanges()) return;

    auto reading = _changes.Take();
    _frame = reading.Frame;
    _changedSinceThoroughPass.Region.Add(reading.Changes.Region);
    if (!reading.Changes.Region.IsEmpty()) SearchWindows(reading.Changes);
  }

  void TextSearch::SearchWindows(const ScreenChanges& changes)
  {
    vector<WindowArea> windows;
    for (auto& window : _windows.Enumerate())
    {
      auto area = VisibleArea(window, _frame->Bounds());
      if (!area.IsEmpty() && changes.Region.Intersects(area)) windows.push_back({ window.Window, area });
    }

    string shown;
    for (auto& text : _quick.Read(_frame, windows, changes))
    {
      if (Matches(text)) return;
      shown += Text(text.Recognized);
    }

    if (!_isShowingThoroughPass) _shown = move(shown);
  }

  bool TextSearch::Matches(const WindowText& text)
  {
    _isFound = _phrase.IsIn(text.Exposed) || _phrase.IsIn(text.Recognized);
    return _isFound;
  }

  bool TextSearch::IsThoroughPassDue() const
  {
    return !_thoroughPass && !_changedSinceThoroughPass.Region.IsEmpty() && steady_clock::now() - _thoroughPassStarted >= _thoroughPassInterval;
  }

  void TextSearch::StartThoroughPass()
  {
    //Changes from now on are for the next pass, which starts from this one's frame
    auto changes = exchange(_changedSinceThoroughPass, { DirtyRegion{ _frame->Bounds() }, _frame });
    _thoroughPassStarted = steady_clock::now();
    _thoroughPass.emplace([this, frame = _frame, changes = move(changes)](stop_token stop) { return _thorough.Read(frame, changes, stop); });
  }

  void TextSearch::CollectThoroughPass()
  {
    if (!_thoroughPass || !_thoroughPass->IsDone()) return;

    auto words = _thoroughPass->Result();
    _thoroughPass.reset();

    _isFound = _phrase.IsIn(words);
    _shown = Text(words);
    _isShowingThoroughPass = true;
  }
}
