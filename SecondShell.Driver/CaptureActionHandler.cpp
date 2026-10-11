#include "pch.h"
#include "CaptureActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Capture;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  const milliseconds _firstFrameTimeout{ 2000 };

  CaptureActionHandler::CaptureActionHandler(dependency_container* container) :
    _capturer(container->resolve<ScreenCapturer>()),
    _encoder(container->resolve<CaptureEncoder>())
  { }

  value_ptr<ActionResult> CaptureActionHandler::Execute(const Action& action, const CancellationToken& /*cancellation*/)
  {
    switch (action.Type())
    {
    case ActionType::CaptureScreen:
      return CaptureRect(static_cast<const CaptureAction&>(action), _capturer->Screen());
    case ActionType::CaptureWindow:
    {
      auto& windowAction = static_cast<const CaptureWindowAction&>(action);
      auto rect = WindowRect(windowAction.Window);
      if (!rect) return ActionResult::Failed(ErrorCode::InvalidArgument, windowAction.Window ? format("There is no visible window {}.", windowAction.Window->to_string()) : "There is no active window.");
      return CaptureRect(windowAction, *rect);
    }
    case ActionType::CaptureRegion:
    {
      auto& regionAction = static_cast<const CaptureRegionAction&>(action);
      return CaptureRect(regionAction, { regionAction.X, regionAction.Y, regionAction.Width, regionAction.Height });
    }
    default:
      return ActionResult::Failed(ErrorCode::ActionFailed, format("{} is not available yet.", action.Name()));
    }
  }

  optional<Rect> CaptureActionHandler::WindowRect(const optional<WindowId>& window) const
  {
    auto handle = window ? reinterpret_cast<HWND>(window->Handle) : GetForegroundWindow();
    if (!handle || !IsWindow(handle) || !IsWindowVisible(handle) || IsIconic(handle)) return nullopt;

    //The visible frame, without the invisible resize borders, clipped to the screen
    RECT bounds;
    if (FAILED(DwmGetWindowAttribute(handle, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds)))) return nullopt;

    auto screen = _capturer->Screen();
    bounds.left = max(bounds.left, LONG(0));
    bounds.top = max(bounds.top, LONG(0));
    bounds.right = min(bounds.right, LONG(screen.Width));
    bounds.bottom = min(bounds.bottom, LONG(screen.Height));
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return nullopt;

    return Rect{ bounds.left, bounds.top, uint32_t(bounds.right - bounds.left), uint32_t(bounds.bottom - bounds.top) };
  }

  value_ptr<ActionResult> CaptureActionHandler::CaptureRect(const CaptureAction& action, const Rect& region)
  {
    Pixels pixels;
    try
    {
      pixels = _capturer->Read(region, _firstFrameTimeout);
    }
    catch (const invalid_argument& exception)
    {
      return ActionResult::Failed(ErrorCode::InvalidArgument, exception.what());
    }

    auto result = make_value<CaptureResult>();
    result->Tag = action.Tag.value_or("");
    result->Image = _encoder->Encode(pixels, action.Resolution, action.Format.value_or(ImageFormat::Png));
    return result;
  }
}
