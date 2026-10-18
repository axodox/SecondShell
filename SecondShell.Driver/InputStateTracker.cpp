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
    _keyboard(container->resolve<IKeyboardInjector>()),
    _touch(container->resolve<ITouchInjector>()),
    _pen(container->resolve<IPenInjector>())
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

  optional<Point> InputStateTracker::TouchPosition(uint32_t id) const
  {
    lock_guard lock{ _mutex };
    return FindTouch(id);
  }

  void InputStateTracker::Touch(uint32_t id, Point position)
  {
    lock_guard lock{ _mutex };
    if (FindTouch(id)) throw logic_error(format("Touch point {} is already down.", id));

    _touchPoints.emplace_back(id, position);
    _touch->Inject(TouchFrame(id, TouchPhase::Down));
  }

  bool InputStateTracker::MoveTouch(uint32_t id, Point position)
  {
    lock_guard lock{ _mutex };
    auto point = ranges::find(_touchPoints, id, &pair<uint32_t, Point>::first);
    if (point == _touchPoints.end()) return false;

    point->second = position;
    _touch->Inject(TouchFrame(id, TouchPhase::Moved));
    return true;
  }

  void InputStateTracker::Lift(uint32_t id)
  {
    lock_guard lock{ _mutex };
    if (!FindTouch(id)) return;

    _touch->Inject(TouchFrame(id, TouchPhase::Up));
    erase_if(_touchPoints, [&](auto& point) { return point.first == id; });
  }

  PenState InputStateTracker::Pen() const
  {
    lock_guard lock{ _mutex };
    return _penState;
  }

  void InputStateTracker::SetPen(const PenState& state)
  {
    lock_guard lock{ _mutex };
    if (state == _penState) return;

    _pen->Inject(state);
    _penState = state;
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

  InputState InputStateTracker::ReleaseTouchPoints()
  {
    lock_guard lock{ _mutex };
    InputState released;
    if (_touchPoints.empty()) return released;

    vector<TouchContact> frame;
    for (auto& [id, position] : _touchPoints) frame.push_back({ id, position, TouchPhase::Up });
    _touch->Inject(frame);

    for (auto& [id, position] : exchange(_touchPoints, {})) released.TouchPoints.push_back(id);
    return released;
  }

  InputState InputStateTracker::ReleasePen()
  {
    lock_guard lock{ _mutex };
    InputState released;
    if (!_penState.IsInRange) return released;

    //Lifted, then out of range: Windows takes no other way out of contact
    if (_penState.IsInContact)
    {
      released.Pen = true;
      _penState.IsInContact = false;
      _penState.IsBarrelPressed = false;
      _pen->Inject(_penState);
    }

    _penState = {};
    _pen->Inject(_penState);
    return released;
  }

  InputState InputStateTracker::ReleaseAll()
  {
    auto released = ReleaseMouseButtons();
    released.Keys = ReleaseKeys().Keys;
    released.TouchPoints = ReleaseTouchPoints().TouchPoints;
    released.Pen = ReleasePen().Pen;
    return released;
  }

  optional<Point> InputStateTracker::FindTouch(uint32_t id) const
  {
    auto point = ranges::find(_touchPoints, id, &pair<uint32_t, Point>::first);
    if (point == _touchPoints.end()) return nullopt;
    return point->second;
  }

  vector<TouchContact> InputStateTracker::TouchFrame(uint32_t id, TouchPhase phase) const
  {
    vector<TouchContact> frame;
    for (auto& [pointId, position] : _touchPoints) frame.push_back({ pointId, position, pointId == id ? phase : TouchPhase::Moved });
    return frame;
  }
}
