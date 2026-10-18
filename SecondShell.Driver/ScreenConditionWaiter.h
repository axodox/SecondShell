#pragma once
#include "CancellationToken.h"
#include "ScreenCapturer.h"
#include "TextRecognizer.h"

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
    std::string LastRecognized;  //what the screen showed at the last check, to tell why the text was not found
  };

  //Waits for the screen: until a text appears on it (Capture.WaitForText), or until a region stops changing
  //(Capture.WaitForStable). Both check at each interval until their timeout or a cancellation.
  class ScreenConditionWaiter
  {
  public:
    explicit ScreenConditionWaiter(Axodox::Infrastructure::dependency_container* container);

    TextWait WaitForText(std::string_view text, const WaitOptions& options, const Rpc::CancellationToken& cancellation);

    //True once the region's pixels have stayed the same for stableFor.
    bool WaitForStable(const Rect& region, std::chrono::milliseconds stableFor, const WaitOptions& options, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<ScreenCapturer> _capturer;
    std::shared_ptr<TextRecognizer> _recognizer;
  };
}
