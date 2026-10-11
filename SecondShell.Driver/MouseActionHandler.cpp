#include "pch.h"
#include "MouseActionHandler.h"
#include "MotionAnimator.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Input;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  //Long enough for applications to see a click as a press and a release; short enough to be a click
  const milliseconds _clickDuration{ 30 };
  const milliseconds _doubleClickInterval{ 60 };

  MouseActionHandler::MouseActionHandler(dependency_container* container) :
    _mouse(container->resolve<IMouseInjector>()),
    _inputState(container->resolve<InputStateTracker>())
  { }

  value_ptr<ActionResult> MouseActionHandler::Execute(const Action& action, const CancellationToken& cancellation)
  {
    switch (action.Type())
    {
    case ActionType::MouseMove:
      return Move(static_cast<const MouseMoveAction&>(action), cancellation);
    case ActionType::MouseClick:
      return Click(static_cast<const MouseClickAction&>(action), cancellation);
    case ActionType::MouseDoubleClick:
      return DoubleClick(static_cast<const MouseDoubleClickAction&>(action), cancellation);
    case ActionType::MouseWheel:
      return Wheel(static_cast<const MouseWheelAction&>(action));
    case ActionType::MousePress:
      return Press(static_cast<const MousePressAction&>(action));
    case ActionType::MouseRelease:
      return Release(static_cast<const MouseReleaseAction&>(action));
    case ActionType::MouseReset:
      return Reset();
    default:
      throw logic_error(format("{} is not a mouse action.", action.Name()));
    }
  }

  value_ptr<ActionResult> MouseActionHandler::Move(const MouseMoveAction& action, const CancellationToken& cancellation)
  {
    Point target{ action.X, action.Y };
    if (auto failure = CheckPosition(target)) return failure;

    auto start = _mouse->Position();
    auto isCompleted = MotionAnimator::Animate(milliseconds(action.Duration.value_or(0)), [&](float progress) {
      _mouse->MoveTo({ start.X + int32_t(lround((target.X - start.X) * progress)), start.Y + int32_t(lround((target.Y - start.Y) * progress)) });
      }, cancellation);

    return isCompleted ? ActionResult::Succeeded() : ActionResult::Cancelled();
  }

  value_ptr<ActionResult> MouseActionHandler::Click(const MouseClickAction& action, const CancellationToken& cancellation)
  {
    if (auto failure = MoveIfGiven(action.X, action.Y)) return failure;

    auto duration = action.Duration ? milliseconds(*action.Duration) : _clickDuration;
    return PressAndRelease(action.Button, duration, cancellation) ? ActionResult::Succeeded() : ActionResult::Cancelled();
  }

  value_ptr<ActionResult> MouseActionHandler::DoubleClick(const MouseDoubleClickAction& action, const CancellationToken& cancellation)
  {
    if (auto failure = MoveIfGiven(action.X, action.Y)) return failure;

    auto isCompleted = PressAndRelease(action.Button, _clickDuration, cancellation) &&
      !cancellation.WaitFor(_doubleClickInterval) &&
      PressAndRelease(action.Button, _clickDuration, cancellation);
    return isCompleted ? ActionResult::Succeeded() : ActionResult::Cancelled();
  }

  value_ptr<ActionResult> MouseActionHandler::Wheel(const MouseWheelAction& action)
  {
    if (auto failure = MoveIfGiven(action.X, action.Y)) return failure;

    _mouse->Wheel(action.Delta, action.Axis);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> MouseActionHandler::Press(const MousePressAction& action)
  {
    _inputState->Press(action.Button);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> MouseActionHandler::Release(const MouseReleaseAction& action)
  {
    _inputState->Release(action.Button);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> MouseActionHandler::Reset()
  {
    _inputState->ReleaseMouseButtons();
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> MouseActionHandler::MoveIfGiven(optional<int32_t> x, optional<int32_t> y)
  {
    if (!x && !y) return nullptr;
    if (!x || !y) return ActionResult::Failed(ErrorCode::InvalidArgument, "Give both x and y, or neither.");

    Point position{ *x, *y };
    if (auto failure = CheckPosition(position)) return failure;

    _mouse->MoveTo(position);
    return nullptr;
  }

  value_ptr<ActionResult> MouseActionHandler::CheckPosition(Point position) const
  {
    auto size = _mouse->ScreenSize();
    if (position.X >= 0 && position.Y >= 0 && position.X < size.X && position.Y < size.Y) return nullptr;

    return ActionResult::Failed(ErrorCode::InvalidArgument, format("({}, {}) is outside the screen, which is {}x{} pixels.", position.X, position.Y, size.X, size.Y));
  }

  bool MouseActionHandler::PressAndRelease(MouseButton button, milliseconds duration, const CancellationToken& cancellation)
  {
    //Released also when cancelled, so a cancelled click does not leave the button held
    _inputState->Press(button);
    auto isCancelled = cancellation.WaitFor(duration);
    _inputState->Release(button);
    return !isCancelled;
  }
}
