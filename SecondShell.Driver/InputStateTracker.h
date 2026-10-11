#pragma once
#include "ActionValues.h"
#include "IKeyboardInjector.h"
#include "IMouseInjector.h"

namespace SecondShell::Input
{
  //Presses and releases buttons and keys, and remembers what is held, so that it can be reported and released
  //(Control.Reset, a failed sequence, the Stop button).
  class InputStateTracker
  {
  public:
    explicit InputStateTracker(Axodox::Infrastructure::dependency_container* container);

    void Press(Actions::MouseButton button);
    void Release(Actions::MouseButton button);
    void Press(uint16_t virtualKey);
    void Release(uint16_t virtualKey);

    //Each returns what it released, in the order it was pressed.
    Actions::InputState ReleaseMouseButtons();
    Actions::InputState ReleaseKeys();
    Actions::InputState ReleaseAll();

  private:
    std::shared_ptr<IMouseInjector> _mouse;
    std::shared_ptr<IKeyboardInjector> _keyboard;

    std::mutex _mutex;
    std::vector<Actions::MouseButton> _mouseButtons;
    std::vector<uint16_t> _keys;
  };
}
