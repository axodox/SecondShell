#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "InputStateTracker.h"
#include "KeyboardActions.h"

namespace SecondShell::Driver
{
  //Performs the Keyboard.* actions.
  class KeyboardActionHandler
  {
  public:
    explicit KeyboardActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Input::IKeyboardInjector> _keyboard;
    std::shared_ptr<Input::InputStateTracker> _inputState;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Type(const Actions::KeyboardTypeAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Tap(const Actions::KeyboardTapAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Press(const Actions::KeyboardPressAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Release(const Actions::KeyboardReleaseAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Reset();

    void TapKey(uint16_t virtualKey);
    void PressWithModifiers(uint16_t virtualKey, std::span<const Actions::KeyModifier> modifiers);
    void ReleaseWithModifiers(uint16_t virtualKey, std::span<const Actions::KeyModifier> modifiers);
  };
}
