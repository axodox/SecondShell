#include "pch.h"
#include "TouchActionHandler.h"
#include "MotionAnimator.h"
#include "ScreenPosition.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Input;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  //Long enough for applications to see a press and a release; short enough to be a tap, not a press and hold
  const milliseconds _tapDuration{ 50 };

  TouchActionHandler::TouchActionHandler(dependency_container* container) :
    _mouse(container->resolve<IMouseInjector>()),
    _inputState(container->resolve<InputStateTracker>())
  { }

  value_ptr<ActionResult> TouchActionHandler::Execute(const Action& action, const CancellationToken& cancellation)
  {
    switch (action.Type())
    {
    case ActionType::TouchPress:
      return Press(static_cast<const TouchPressAction&>(action));
    case ActionType::TouchRelease:
      return Release(static_cast<const TouchReleaseAction&>(action));
    case ActionType::TouchTap:
      return Tap(static_cast<const TouchTapAction&>(action), cancellation);
    case ActionType::TouchMove:
      return Move(static_cast<const TouchMoveAction&>(action), cancellation);
    case ActionType::TouchReset:
      return Reset();
    default:
      throw logic_error(format("{} is not a touch action.", action.Name()));
    }
  }

  value_ptr<ActionResult> TouchActionHandler::Press(const TouchPressAction& action)
  {
    Point position{ action.X, action.Y };
    if (auto failure = CheckPress(action.Id, position)) return failure;

    _inputState->Touch(action.Id, position);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> TouchActionHandler::Release(const TouchReleaseAction& action)
  {
    if (!_inputState->TouchPosition(action.Id)) return ActionResult::Failed(ErrorCode::InvalidArgument, format("Touch point {} is not down.", action.Id));

    _inputState->Lift(action.Id);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> TouchActionHandler::Tap(const TouchTapAction& action, const CancellationToken& cancellation)
  {
    Point position{ action.X, action.Y };
    if (auto failure = CheckPress(action.Id, position)) return failure;

    //Lifted also when cancelled, so a cancelled tap does not leave the finger down
    _inputState->Touch(action.Id, position);
    auto isCancelled = cancellation.WaitFor(_tapDuration);
    _inputState->Lift(action.Id);
    return isCancelled ? ActionResult::Cancelled() : ActionResult::Succeeded();
  }

  value_ptr<ActionResult> TouchActionHandler::Move(const TouchMoveAction& action, const CancellationToken& cancellation)
  {
    Point target{ action.X, action.Y };
    if (auto failure = CheckOnScreen(target, _mouse->ScreenSize())) return failure;

    auto start = _inputState->TouchPosition(action.Id);
    if (!start) return ActionResult::Failed(ErrorCode::InvalidArgument, format("Touch point {} is not down; Touch.Press it first.", action.Id));

    auto isLifted = false;
    auto isCompleted = MotionAnimator::Animate(milliseconds(action.Duration.value_or(0)), [&](float progress) {
      Point position{ start->X + int32_t(lround((target.X - start->X) * progress)), start->Y + int32_t(lround((target.Y - start->Y) * progress)) };
      if (!isLifted) isLifted = !_inputState->MoveTouch(action.Id, position);
      }, cancellation);

    if (isLifted) return ActionResult::Failed(ErrorCode::ActionFailed, format("Touch point {} was lifted while it moved.", action.Id));
    return isCompleted ? ActionResult::Succeeded() : ActionResult::Cancelled();
  }

  value_ptr<ActionResult> TouchActionHandler::Reset()
  {
    _inputState->ReleaseTouchPoints();
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> TouchActionHandler::CheckPress(uint32_t id, Point position) const
  {
    if (_inputState->TouchPosition(id)) return ActionResult::Failed(ErrorCode::InvalidArgument, format("Touch point {} is already down; release it or use another id.", id));
    return CheckOnScreen(position, _mouse->ScreenSize());
  }
}
