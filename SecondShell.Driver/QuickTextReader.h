#pragma once
#include "AreaTextReader.h"
#include "IWindowTextSource.h"

namespace SecondShell::Capture
{
  //A window, and the part of the screen it shows.
  struct WindowArea
  {
    Actions::WindowId Window;
    Rect Area;
  };

  //A window's text, once from each source.
  struct WindowText
  {
    std::vector<ScreenWord> Exposed;
    std::vector<ScreenWord> Recognized;
  };

  //The quick tier of the text waits: for each window, the text it exposes (UI Automation), and what the quick
  //recognizer reads in the window's area, all windows in one go. Both are kept with the fingerprint of the window's
  //pixels, so an unchanged window costs only that.
  class QuickTextReader
  {
  public:
    QuickTextReader(std::shared_ptr<IWindowTextSource> exposedText, std::shared_ptr<ITextRecognizer> recognizer);

    //The text of each window, in their order.
    std::vector<WindowText> Read(const std::shared_ptr<const ScreenFrame>& frame, std::span<const WindowArea> windows, const ScreenChanges& changes);

  private:
    std::shared_ptr<IWindowTextSource> _exposedText;
    ScreenTextCache _exposedTexts;
    AreaTextReader _recognizedTexts;

    std::vector<ScreenWord> ReadExposed(const Pixels& screen, const WindowArea& window);
  };
}
