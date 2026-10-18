#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "InputStateTracker.h"
#include "PenActions.h"

namespace SecondShell::Driver
{
  //Performs the Pen.* actions.
  class PenActionHandler
  {
  public:
    explicit PenActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Input::IMouseInjector> _mouse;  //for the screen's size
    std::shared_ptr<Input::InputStateTracker> _inputState;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Move(const Actions::PenMoveAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Press(const Actions::PenPressAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Release();
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Tap(const Actions::PenTapAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Reset();

    //Hovers at the position, then touches the screen as given.
    void TouchDown(Input::PenState contact);
  };
}
