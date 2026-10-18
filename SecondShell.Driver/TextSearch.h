#pragma once
#include "LowPriorityTask.h"
#include "PhraseMatcher.h"
#include "QuickTextReader.h"
#include "ScreenChangeTracker.h"
#include "ThoroughTextReader.h"
#include "WindowManager.h"

namespace SecondShell::Capture
{
  //One wait for a text (OcrOptions.md, Decided design for text waits). It starts with the quick tier on every window,
  //then runs the thorough tier over the whole screen in the background. After that, each check runs the quick tier on
  //the windows which changed, and the thorough tier again once its interval has passed, if anything changed since.
  class TextSearch
  {
  public:
    TextSearch(std::string_view phrase, ScreenCapturer& capturer, const Desktop::WindowManager& windows, QuickTextReader& quick, ThoroughTextReader& thorough);

    void Start();
    void Check();

    bool IsFound() const;

    //What the screen showed at the last reading, to tell a misread from a missing text.
    const std::string& Shown() const;

  private:
    PhraseMatcher _phrase;
    const Desktop::WindowManager& _windows;
    QuickTextReader& _quick;
    ThoroughTextReader& _thorough;
    ScreenChangeTracker _changes;

    bool _isFound = false;
    std::string _shown;
    bool _isShowingThoroughPass = false;

    std::shared_ptr<const ScreenFrame> _frame;  //the newest snapshot
    ScreenChanges _changedSinceThoroughPass;
    std::chrono::steady_clock::time_point _thoroughPassStarted;
    std::optional<LowPriorityTask<std::vector<ScreenWord>>> _thoroughPass;  //last, so it stops first

    void SearchChanges();
    //The windows which changed.
    void SearchWindows(const ScreenChanges& changes);
    bool Matches(const WindowText& text);

    bool IsThoroughPassDue() const;
    void StartThoroughPass();
    void CollectThoroughPass();
  };
}
