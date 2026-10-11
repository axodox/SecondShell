#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "InputStateTracker.h"
#include "MouseActions.h"

namespace SecondShell::Driver
{
  //Performs the Mouse.* actions.
  class MouseActionHandler
  {
  public:
    explicit MouseActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Input::IMouseInjector> _mouse;
    std::shared_ptr<Input::InputStateTracker> _inputState;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Move(const Actions::MouseMoveAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Click(const Actions::MouseClickAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> DoubleClick(const Actions::MouseDoubleClickAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Wheel(const Actions::MouseWheelAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Press(const Actions::MousePressAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Release(const Actions::MouseReleaseAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Reset();

    //Moves to (x, y) if given; a failed result if the position is invalid.
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> MoveIfGiven(std::optional<int32_t> x, std::optional<int32_t> y);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> CheckPosition(Input::Point position) const;
    bool PressAndRelease(Actions::MouseButton button, std::chrono::milliseconds duration, const Rpc::CancellationToken& cancellation);
  };
}
