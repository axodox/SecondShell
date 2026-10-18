#pragma once
#include "ITextRecognizer.h"
#include "IWindowTextSource.h"
#include "ScreenTextCache.h"

namespace SecondShell::Capture
{
  //A window's text, once from each source.
  struct WindowText
  {
    std::vector<ScreenWord> Exposed;
    std::vector<ScreenWord> Recognized;
  };

  //The quick tier of the text waits, one window at a time: the text the window exposes (UI Automation), and what the
  //quick recognizer reads in the window's crop. Both are cached with the fingerprint of the window's pixels, so an
  //unchanged window costs only that.
  class QuickTextReader
  {
  public:
    QuickTextReader(std::shared_ptr<IWindowTextSource> exposedText, std::shared_ptr<ITextRecognizer> recognizer);

    //The text of the window, which shows the area of the screen.
    WindowText Read(const Pixels& screen, Actions::WindowId window, const Rect& area);

  private:
    std::shared_ptr<IWindowTextSource> _exposedText;
    std::shared_ptr<ITextRecognizer> _recognizer;
    ScreenTextCache _exposedTexts;
    ScreenTextCache _recognizedTexts;
  };
}
