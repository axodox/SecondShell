#include "pch.h"
#include "PenActionHandler.h"
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
  const milliseconds _tapDuration{ 50 };
  const float _defaultPressure = 0.5f;

  namespace
  {
    int32_t Interpolate(int32_t from, int32_t to, float progress)
    {
      return from + int32_t(lround((to - from) * progress));
    }

    float Interpolate(float from, float to, float progress)
    {
      return from + (to - from) * progress;
    }

    //The way round the circle that is shorter
    uint32_t InterpolateAngle(uint32_t from, uint32_t to, float progress)
    {
      auto difference = (int32_t(to) - int32_t(from) + 540) % 360 - 180;
      return uint32_t((int32_t(from) + Interpolate(0, difference, progress) + 360) % 360);
    }
  }

  PenActionHandler::PenActionHandler(dependency_container* container) :
    _mouse(container->resolve<IMouseInjector>()),
    _inputState(container->resolve<InputStateTracker>())
  { }

  value_ptr<ActionResult> PenActionHandler::Execute(const Action& action, const CancellationToken& cancellation)
  {
    switch (action.Type())
    {
    case ActionType::PenMove:
      return Move(static_cast<const PenMoveAction&>(action), cancellation);
    case ActionType::PenPress:
      return Press(static_cast<const PenPressAction&>(action));
    case ActionType::PenRelease:
      return Release();
    case ActionType::PenTap:
      return Tap(static_cast<const PenTapAction&>(action), cancellation);
    case ActionType::PenReset:
      return Reset();
    default:
      throw logic_error(format("{} is not a pen action.", action.Name()));
    }
  }

  value_ptr<ActionResult> PenActionHandler::Move(const PenMoveAction& action, const CancellationToken& cancellation)
  {
    Point target{ action.X, action.Y };
    if (auto failure = CheckOnScreen(target, _mouse->ScreenSize())) return failure;

    //Out of range, it comes into range above the target
    auto start = _inputState->Pen();
    if (!start.IsInRange)
    {
      start.IsInRange = true;
      start.Position = target;
      _inputState->SetPen(start);
    }

    auto end = start;
    end.Position = target;
    end.Pressure = action.Pressure.value_or(start.Pressure);
    end.TiltX = action.TiltX.value_or(start.TiltX);
    end.TiltY = action.TiltY.value_or(start.TiltY);
    end.Rotation = action.Rotation.value_or(start.Rotation);

    auto isCompleted = MotionAnimator::Animate(milliseconds(action.Duration.value_or(0)), [&](float progress) {
      auto state = start;
      state.Position = { Interpolate(start.Position.X, end.Position.X, progress), Interpolate(start.Position.Y, end.Position.Y, progress) };
      state.Pressure = Interpolate(start.Pressure, end.Pressure, progress);
      state.TiltX = Interpolate(start.TiltX, end.TiltX, progress);
      state.TiltY = Interpolate(start.TiltY, end.TiltY, progress);
      state.Rotation = InterpolateAngle(start.Rotation, end.Rotation, progress);
      _inputState->SetPen(state);
      }, cancellation);

    return isCompleted ? ActionResult::Succeeded() : ActionResult::Cancelled();
  }

  value_ptr<ActionResult> PenActionHandler::Press(const PenPressAction& action)
  {
    if (action.X.has_value() != action.Y.has_value()) return ActionResult::Failed(ErrorCode::InvalidArgument, "Give both x and y, or neither.");

    auto contact = _inputState->Pen();
    if (contact.IsInContact) return ActionResult::Failed(ErrorCode::InvalidArgument, "The pen is already touching the screen; Pen.Release it first.");

    if (action.X)
    {
      contact.Position = { *action.X, *action.Y };
      if (auto failure = CheckOnScreen(contact.Position, _mouse->ScreenSize())) return failure;
    }
    else if (!contact.IsInRange)
    {
      return ActionResult::Failed(ErrorCode::InvalidArgument, "The pen is not over the screen; give x and y, or Pen.Move it there first.");
    }

    contact.IsInContact = true;
    contact.Pressure = action.Pressure.value_or(_defaultPressure);
    contact.TiltX = action.TiltX.value_or(0);
    contact.TiltY = action.TiltY.value_or(0);
    contact.Rotation = action.Rotation.value_or(0);
    contact.IsBarrelPressed = action.Barrel.value_or(false);
    contact.IsEraser = action.Eraser.value_or(false);
    TouchDown(contact);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> PenActionHandler::Release()
  {
    auto state = _inputState->Pen();
    if (!state.IsInContact) return ActionResult::Failed(ErrorCode::InvalidArgument, "The pen is not touching the screen.");

    state.IsInContact = false;
    state.IsBarrelPressed = false;
    state.Pressure = 0.f;
    _inputState->SetPen(state);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> PenActionHandler::Tap(const PenTapAction& action, const CancellationToken& cancellation)
  {
    Point position{ action.X, action.Y };
    if (auto failure = CheckOnScreen(position, _mouse->ScreenSize())) return failure;

    auto contact = _inputState->Pen();
    if (contact.IsInContact) return ActionResult::Failed(ErrorCode::InvalidArgument, "The pen is already touching the screen; Pen.Release it first.");

    contact.Position = position;
    contact.IsInContact = true;
    contact.Pressure = action.Pressure.value_or(_defaultPressure);
    contact.IsBarrelPressed = action.Barrel.value_or(false);
    contact.IsEraser = action.Eraser.value_or(false);
    TouchDown(contact);

    //Lifted also when cancelled, so a cancelled tap does not leave the pen down
    auto isCancelled = cancellation.WaitFor(_tapDuration);
    Release();
    return isCancelled ? ActionResult::Cancelled() : ActionResult::Succeeded();
  }

  value_ptr<ActionResult> PenActionHandler::Reset()
  {
    _inputState->ReleasePen();
    return ActionResult::Succeeded();
  }

  void PenActionHandler::TouchDown(PenState contact)
  {
    //Over the point first, with the end and the button it touches with, as a hand would bring it
    contact.IsInRange = true;
    auto hover = contact;
    hover.IsInContact = false;
    hover.Pressure = 0.f;
    if (hover != _inputState->Pen()) _inputState->SetPen(hover);

    _inputState->SetPen(contact);
  }
}
