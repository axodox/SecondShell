#include "pch.h"
#include "WindowActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Driver
{
  WindowActionHandler::WindowActionHandler(dependency_container* container) :
    _windows(container->resolve<WindowManager>())
  { }

  value_ptr<ActionResult> WindowActionHandler::Execute(const Action& action, const CancellationToken& /*cancellation*/)
  {
    try
    {
      return Run(action);
    }
    catch (const invalid_argument& exception)
    {
      return ActionResult::Failed(ErrorCode::InvalidArgument, exception.what());
    }
  }

  value_ptr<ActionResult> WindowActionHandler::Run(const Action& action)
  {
    switch (action.Type())
    {
    case ActionType::WindowEnumerate:
      return Enumerate();
    case ActionType::WindowFocus:
      return Focus(static_cast<const WindowFocusAction&>(action));
    case ActionType::WindowMove:
    {
      auto& move = static_cast<const WindowMoveAction&>(action);
      _windows->Move(move.Window, move.X, move.Y);
      return ActionResult::Succeeded();
    }
    case ActionType::WindowResize:
    {
      auto& resize = static_cast<const WindowResizeAction&>(action);
      _windows->Resize(resize.Window, resize.Width, resize.Height);
      return ActionResult::Succeeded();
    }
    case ActionType::WindowMinimize:
      return SetState(static_cast<const WindowMinimizeAction&>(action).Window, WindowState::Minimized);
    case ActionType::WindowMaximize:
      return SetState(static_cast<const WindowMaximizeAction&>(action).Window, WindowState::Maximized);
    case ActionType::WindowRestore:
      return SetState(static_cast<const WindowRestoreAction&>(action).Window, WindowState::Normal);
    case ActionType::WindowClose:
      _windows->Close(static_cast<const WindowCloseAction&>(action).Window);
      return ActionResult::Succeeded();
    default:
      throw logic_error(format("{} is not a window action.", action.Name()));
    }
  }

  value_ptr<ActionResult> WindowActionHandler::Enumerate()
  {
    auto result = make_value<WindowListResult>();
    result->Windows = _windows->Enumerate();
    return result;
  }

  value_ptr<ActionResult> WindowActionHandler::Focus(const WindowFocusAction& action)
  {
    if (!_windows->Focus(action.Window)) return ActionResult::Failed(ErrorCode::ActionFailed, format("Windows did not switch the foreground to window {}.", action.Window.to_string()));
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> WindowActionHandler::SetState(WindowId window, WindowState state)
  {
    if (!_windows->SetState(window, state)) return ActionResult::Failed(ErrorCode::ActionFailed, format("Window {} did not become {}.", window.to_string(), to_string(state)));
    return ActionResult::Succeeded();
  }
}
