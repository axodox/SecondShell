#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "CaptureActions.h"
#include "CaptureEncoder.h"
#include "ScreenCapturer.h"

namespace SecondShell::Driver
{
  //Performs the Capture.* actions. The host decides where a capture goes (inline or file) and saves files; this returns
  //the encoded image.
  class CaptureActionHandler
  {
  public:
    explicit CaptureActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Capture::ScreenCapturer> _capturer;
    std::shared_ptr<Capture::CaptureEncoder> _encoder;

    std::optional<Capture::Rect> WindowRect(const std::optional<Actions::WindowId>& window) const;
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> CaptureRect(const Actions::CaptureAction& action, const Capture::Rect& region);
  };
}
