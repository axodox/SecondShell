#include "pch.h"
#include "TextSearch.h"
#include "TextLines.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Capture
{
  const milliseconds _frameTimeout{ 2000 };
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
    _capturer(capturer),
    _windows(windows),
    _quick(quick),
    _thorough(thorough),
    _changes(capturer)
  { }

  void TextSearch::Start()
  {
    auto screen = ReadScreen();
    _changes.Take(screen);
    _changedSinceThoroughPass = DirtyRegion{ screen.Bounds() };

    SearchWindows(screen, nullptr);
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

  Pixels TextSearch::ReadScreen() const
  {
    return _capturer.Read(_capturer.Screen(), _frameTimeout);
  }

  void TextSearch::SearchChanges()
  {
    if (!_changes.HasChanges()) return;

    auto screen = ReadScreen();
    auto changes = _changes.Take(screen);
    _changedSinceThoroughPass.Add(changes);
    if (!changes.IsEmpty()) SearchWindows(screen, &changes);
  }

  void TextSearch::SearchWindows(const Pixels& screen, const DirtyRegion* changes)
  {
    string shown;
    for (auto& window : _windows.Enumerate())
    {
      auto area = VisibleArea(window, screen.Bounds());
      if (area.IsEmpty() || (changes && !changes->Intersects(area))) continue;

      auto text = _quick.Read(screen, window.Window, area);
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
    return !_thoroughPass && !_changedSinceThoroughPass.IsEmpty() && steady_clock::now() - _thoroughPassStarted >= _thoroughPassInterval;
  }

  void TextSearch::StartThoroughPass()
  {
    //Changes from now on are for the next pass
    _changedSinceThoroughPass.Clear();
    _thoroughPassStarted = steady_clock::now();
    _thoroughPass.emplace([this](stop_token stop) { return _thorough.Read(ReadScreen(), stop); });
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
