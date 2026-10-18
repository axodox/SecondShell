#pragma once
#include "CancellationToken.h"
#include "QuickTextReader.h"
#include "ScreenCapturer.h"
#include "ThoroughTextReader.h"
#include "WindowManager.h"

namespace SecondShell::Capture
{
  struct WaitOptions
  {
    std::chrono::milliseconds Interval;
    std::chrono::milliseconds Timeout;
  };

  struct TextWait
  {
    bool IsFound = false;
    std::string LastRecognized;  //what the screen showed at the last reading, to tell why the text was not found
  };

  //Waits for the screen: until a text appears on it (Capture.WaitForText, see TextSearch), or until a region stops
  //changing (Capture.WaitForStable). Both check at each interval until their timeout or a cancellation.
  class ScreenConditionWaiter
  {
  public:
    explicit ScreenConditionWaiter(Axodox::Infrastructure::dependency_container* container);

    TextWait WaitForText(std::string_view text, const WaitOptions& options, const Rpc::CancellationToken& cancellation);

    //True once the region's pixels have stayed the same for stableFor.
    bool WaitForStable(const Rect& region, std::chrono::milliseconds stableFor, const WaitOptions& options, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<ScreenCapturer> _capturer;
    std::shared_ptr<Desktop::WindowManager> _windows;
    std::shared_ptr<QuickTextReader> _quickReader;
    std::shared_ptr<ThoroughTextReader> _thoroughReader;
  };
}
