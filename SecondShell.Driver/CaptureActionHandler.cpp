#include "pch.h"
#include "CaptureActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Capture;
using namespace SecondShell::Desktop;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  const milliseconds _firstFrameTimeout{ 2000 };

  //Design.md, Capture actions
  const milliseconds _textInterval{ 500 };
  const milliseconds _stableInterval{ 100 };
  const milliseconds _stableFor{ 500 };
  const size_t _recognizedTextLimit = 500;

  CaptureActionHandler::CaptureActionHandler(dependency_container* container) :
    _capturer(container->resolve<ScreenCapturer>()),
    _encoder(container->resolve<CaptureEncoder>()),
    _windows(container->resolve<WindowManager>()),
    _waiter(container->resolve<ScreenConditionWaiter>())
  { }

  value_ptr<ActionResult> CaptureActionHandler::Execute(const Action& action, const CancellationToken& cancellation)
  {
    try
    {
      return Run(action, cancellation);
    }
    catch (const invalid_argument& exception)
    {
      return ActionResult::Failed(ErrorCode::InvalidArgument, exception.what());
    }
  }

  value_ptr<ActionResult> CaptureActionHandler::Run(const Action& action, const CancellationToken& cancellation)
  {
    switch (action.Type())
    {
    case ActionType::CaptureScreen:
      return CaptureRect(static_cast<const CaptureAction&>(action), _capturer->Screen());
    case ActionType::CaptureWindow:
    {
      auto& windowAction = static_cast<const CaptureWindowAction&>(action);
      return CaptureRect(windowAction, WindowRect(windowAction.Window));
    }
    case ActionType::CaptureRegion:
    {
      auto& regionAction = static_cast<const CaptureRegionAction&>(action);
      return CaptureRect(regionAction, { regionAction.X, regionAction.Y, regionAction.Width, regionAction.Height });
    }
    case ActionType::CaptureWaitForText:
      return WaitForText(static_cast<const CaptureWaitForTextAction&>(action), cancellation);
    case ActionType::CaptureWaitForStable:
      return WaitForStable(static_cast<const CaptureWaitForStableAction&>(action), cancellation);
    default:
      throw logic_error(format("{} is not a capture action.", action.Name()));
    }
  }

  value_ptr<ActionResult> CaptureActionHandler::WaitForText(const CaptureWaitForTextAction& action, const CancellationToken& cancellation)
  {
    if (action.Text.empty()) throw invalid_argument("The text to wait for is empty.");

    auto timeout = action.WaitTimeout().value_or(Action::DefaultWaitTimeout);
    auto wait = _waiter->WaitForText(action.Text, { .Interval = action.Interval ? milliseconds(*action.Interval) : _textInterval, .Timeout = timeout }, cancellation);
    if (wait.IsFound) return ActionResult::Succeeded();
    if (cancellation.IsCancelled()) return ActionResult::Cancelled();

    //What OCR saw helps to tell a misread from a missing text
    auto recognized = wait.LastRecognized.substr(0, _recognizedTextLimit);
    replace(recognized.begin(), recognized.end(), '\n', ' ');
    return ActionResult::Failed(ErrorCode::Timeout, format("The text \"{}\" did not appear within {} ms. The screen showed: {}", action.Text, timeout.count(), recognized));
  }

  value_ptr<ActionResult> CaptureActionHandler::WaitForStable(const CaptureWaitForStableAction& action, const CancellationToken& cancellation)
  {
    Rect region;
    if (action.Window)
    {
      region = WindowRect(action.Window);
    }
    else if (action.X || action.Y || action.Width || action.Height)
    {
      if (!action.X || !action.Y || !action.Width || !action.Height) throw invalid_argument("A region needs x, y, width and height.");
      region = { *action.X, *action.Y, *action.Width, *action.Height };
    }
    else
    {
      region = _capturer->Screen();
    }

    auto timeout = action.WaitTimeout().value_or(Action::DefaultWaitTimeout);
    auto stableFor = action.Stable ? milliseconds(*action.Stable) : _stableFor;
    if (_waiter->WaitForStable(region, stableFor, { .Interval = action.Interval ? milliseconds(*action.Interval) : _stableInterval, .Timeout = timeout }, cancellation)) return ActionResult::Succeeded();
    if (cancellation.IsCancelled()) return ActionResult::Cancelled();

    return ActionResult::Failed(ErrorCode::Timeout, format("The screen did not stay the same for {} ms within {} ms; an animation, such as a caret or a spinner, can keep it changing, so limit the check to a window or region.", stableFor.count(), timeout.count()));
  }

  Rect CaptureActionHandler::WindowRect(const optional<WindowId>& window) const
  {
    auto target = window ? window : _windows->Foreground();
    if (!target) throw invalid_argument("There is no active window.");

    auto info = _windows->Get(*target);
    if (info.State == WindowState::Minimized) throw invalid_argument(format("Window {} is minimized; Window.Restore shows it.", target->to_string()));

    //Clipped to the screen
    auto screen = _capturer->Screen();
    auto left = max(info.X, 0);
    auto top = max(info.Y, 0);
    auto right = min(info.X + int32_t(info.Width), int32_t(screen.Width));
    auto bottom = min(info.Y + int32_t(info.Height), int32_t(screen.Height));
    if (right <= left || bottom <= top) throw invalid_argument(format("Window {} is not on the screen.", target->to_string()));

    return Rect{ left, top, uint32_t(right - left), uint32_t(bottom - top) };
  }

  value_ptr<ActionResult> CaptureActionHandler::CaptureRect(const CaptureAction& action, const Rect& region)
  {
    auto pixels = _capturer->Read(region, _firstFrameTimeout);

    auto result = make_value<CaptureResult>();
    result->Tag = action.Tag.value_or("");
    result->Image = _encoder->Encode(pixels, action.Resolution, action.Format.value_or(ImageFormat::Png));
    return result;
  }
}
