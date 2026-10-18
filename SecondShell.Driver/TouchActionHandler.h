#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "InputStateTracker.h"
#include "TouchActions.h"

namespace SecondShell::Driver
{
  //Performs the Touch.* actions.
  class TouchActionHandler
  {
  public:
    explicit TouchActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Input::IMouseInjector> _mouse;  //for the screen's size
    std::shared_ptr<Input::InputStateTracker> _inputState;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Press(const Actions::TouchPressAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Release(const Actions::TouchReleaseAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Tap(const Actions::TouchTapAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Move(const Actions::TouchMoveAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Reset();

    //A failed result if the touch point cannot go down at the position.
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> CheckPress(uint32_t id, Input::Point position) const;
  };
}
