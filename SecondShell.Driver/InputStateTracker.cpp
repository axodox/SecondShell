#include "pch.h"
#include "InputStateTracker.h"
#include "KeyNames.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Input
{
  InputStateTracker::InputStateTracker(dependency_container* container) :
    _mouse(container->resolve<IMouseInjector>()),
    _keyboard(container->resolve<IKeyboardInjector>())
  { }

  void InputStateTracker::Press(MouseButton button)
  {
    lock_guard lock{ _mutex };
    _mouse->Press(button);
    if (ranges::find(_mouseButtons, button) == _mouseButtons.end()) _mouseButtons.push_back(button);
  }

  void InputStateTracker::Release(MouseButton button)
  {
    lock_guard lock{ _mutex };
    _mouse->Release(button);
    erase(_mouseButtons, button);
  }

  void InputStateTracker::Press(uint16_t virtualKey)
  {
    lock_guard lock{ _mutex };
    _keyboard->Press(virtualKey);
    if (ranges::find(_keys, virtualKey) == _keys.end()) _keys.push_back(virtualKey);
  }

  void InputStateTracker::Release(uint16_t virtualKey)
  {
    lock_guard lock{ _mutex };
    _keyboard->Release(virtualKey);
    erase(_keys, virtualKey);
  }

  InputState InputStateTracker::ReleaseMouseButtons()
  {
    lock_guard lock{ _mutex };
    InputState released;
    for (auto button : _mouseButtons | views::reverse) _mouse->Release(button);

    released.MouseButtons = exchange(_mouseButtons, {});
    return released;
  }

  InputState InputStateTracker::ReleaseKeys()
  {
    lock_guard lock{ _mutex };
    InputState released;
    for (auto key : _keys | views::reverse) _keyboard->Release(key);

    for (auto key : exchange(_keys, {})) released.Keys.push_back(KeyNames::ToString(key));
    return released;
  }

  InputState InputStateTracker::ReleaseAll()
  {
    auto released = ReleaseMouseButtons();
    released.Keys = ReleaseKeys().Keys;
    return released;
  }
}
