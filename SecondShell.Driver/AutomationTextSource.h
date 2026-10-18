#pragma once
#include "IWindowTextSource.h"

namespace SecondShell::Capture
{
  //The text a window exposes to UI Automation: what text controls show (TextPattern, e.g. consoles and editors), and
  //elsewhere each element's name and value. One cached request per window fetches all of it, with timeouts, as an
  //application may not answer. Called on the waits' threads, which are in the multithreaded apartment.
  class AutomationTextSource : public IWindowTextSource
  {
    inline static const Axodox::Infrastructure::logger _logger{ "AutomationTextSource" };

  public:
    AutomationTextSource();

    std::vector<ScreenWord> Read(Actions::WindowId window) const override;

  private:
    winrt::com_ptr<IUIAutomation2> _automation;
    winrt::com_ptr<IUIAutomationCacheRequest> _request;
    winrt::com_ptr<IUIAutomationCondition> _shownElements;

    std::vector<ScreenWord> ReadElements(HWND window) const;

    static bool HasTextPattern(IUIAutomationElement* element);
    static void AddShownText(IUIAutomationElement* element, std::vector<ScreenWord>& words);
    static void AddNameAndValue(IUIAutomationElement* element, std::vector<ScreenWord>& words);
  };
}
