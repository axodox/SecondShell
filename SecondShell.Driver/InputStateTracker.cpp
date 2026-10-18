#include "pch.h"
#include "InputStateTracker.h"
#include "KeyNames.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Input
{
  //Unassigned: a press of it between a Windows or Alt key's down and up makes a shortcut which does nothing (AutoHotkey's
  //menu mask key)
  const uint16_t _menuMaskKey = 0xE8;

  namespace
  {
    //Released alone, these open the Start menu or the focused window's menu bar
    bool IsMenuKey(uint16_t virtualKey)
    {
      switch (virtualKey)
      {
      case VK_LWIN:
      case VK_RWIN:
      case VK_MENU:
      case VK_LMENU:
      case VK_RMENU:
        return true;
      default:
        return false;
      }
    }
  }

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
    if (ranges::any_of(_keys, IsMenuKey))
    {
      _keyboard->Press(_menuMaskKey);
      _keyboard->Release(_menuMaskKey);
    }
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
