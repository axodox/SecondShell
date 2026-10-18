#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "CaptureActions.h"
#include "CaptureEncoder.h"
#include "ScreenCapturer.h"
#include "ScreenConditionWaiter.h"
#include "WindowManager.h"

namespace SecondShell::Driver
{
  //Performs the Capture.* actions. The host decides where a capture goes (inline or file) and saves files; this returns
  //the encoded image. The waits watch the screen until their condition holds.
  class CaptureActionHandler
  {
  public:
    explicit CaptureActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Capture::ScreenCapturer> _capturer;
    std::shared_ptr<Capture::CaptureEncoder> _encoder;
    std::shared_ptr<Desktop::WindowManager> _windows;
    std::shared_ptr<Capture::ScreenConditionWaiter> _waiter;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Run(const Actions::Action& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> WaitForText(const Actions::CaptureWaitForTextAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> WaitForStable(const Actions::CaptureWaitForStableAction& action, const Rpc::CancellationToken& cancellation);

    //The window's visible part on the screen; throws invalid_argument if it has none.
    Capture::Rect WindowRect(const std::optional<Actions::WindowId>& window) const;
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> CaptureRect(const Actions::CaptureAction& action, const Capture::Rect& region);
  };
}
