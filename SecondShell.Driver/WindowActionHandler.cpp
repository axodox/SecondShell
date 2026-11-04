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
    if (action.Type() == ActionType::WindowEnumerate) return Enumerate(static_cast<const WindowEnumerateAction&>(action));

    auto window = _windows->Find(static_cast<const WindowTargetAction&>(action).Query());
    switch (action.Type())
    {
    case ActionType::WindowFocus:
      return Focus(window);
    case ActionType::WindowMove:
    {
      auto& move = static_cast<const WindowMoveAction&>(action);
      _windows->Move(window, move.X, move.Y);
      return ActionResult::Succeeded();
    }
    case ActionType::WindowResize:
    {
      auto& resize = static_cast<const WindowResizeAction&>(action);
      _windows->Resize(window, resize.Width, resize.Height);
      return ActionResult::Succeeded();
    }
    case ActionType::WindowMinimize:
      return SetState(window, WindowState::Minimized);
    case ActionType::WindowMaximize:
      return SetState(window, WindowState::Maximized);
    case ActionType::WindowRestore:
      return SetState(window, WindowState::Normal);
    case ActionType::WindowClose:
      _windows->Close(window);
      return ActionResult::Succeeded();
    default:
      throw logic_error(format("{} is not a window action.", action.Name()));
    }
  }

  value_ptr<ActionResult> WindowActionHandler::Enumerate(const WindowEnumerateAction& action)
  {
    auto result = make_value<WindowListResult>();
    result->Windows = _windows->Enumerate(action.All.value_or(false));
    return result;
  }

  value_ptr<ActionResult> WindowActionHandler::Focus(WindowId window)
  {
    if (!_windows->Focus(window)) return ActionResult::Failed(ErrorCode::ActionFailed, format("Windows did not switch the foreground to window {}.", window.to_string()));
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> WindowActionHandler::SetState(WindowId window, WindowState state)
  {
    if (!_windows->SetState(window, state)) return ActionResult::Failed(ErrorCode::ActionFailed, format("Window {} did not become {}.", window.to_string(), to_string(state)));
    return ActionResult::Succeeded();
  }
}
