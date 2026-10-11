#include "pch.h"
#include "KeyboardActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Input;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Driver
{
  //Some applications drop characters typed faster than they process them
  const milliseconds _defaultTypingPause{ 5 };

  namespace
  {
    uint16_t VirtualKey(KeyModifier modifier)
    {
      switch (modifier)
      {
      case KeyModifier::Shift:
        return VK_SHIFT;
      case KeyModifier::Ctrl:
        return VK_CONTROL;
      case KeyModifier::Alt:
        return VK_MENU;
      case KeyModifier::Win:
        return VK_LWIN;
      default:
        throw logic_error("Unknown key modifier.");
      }
    }

    u32string ToCodePoints(string_view utf8)
    {
      auto utf16 = winrt::to_hstring(utf8);

      u32string result;
      for (uint32_t index = 0; index < utf16.size(); index++)
      {
        char32_t unit = utf16[index];
        if (unit >= 0xD800 && unit < 0xDC00 && index + 1 < utf16.size())
        {
          unit = 0x10000 + ((unit - 0xD800) << 10) + (utf16[++index] - 0xDC00);
        }
        result.push_back(unit);
      }
      return result;
    }
  }

  KeyboardActionHandler::KeyboardActionHandler(dependency_container* container) :
    _keyboard(container->resolve<IKeyboardInjector>()),
    _inputState(container->resolve<InputStateTracker>())
  { }

  value_ptr<ActionResult> KeyboardActionHandler::Execute(const Action& action, const CancellationToken& cancellation)
  {
    switch (action.Type())
    {
    case ActionType::KeyboardType:
      return Type(static_cast<const KeyboardTypeAction&>(action), cancellation);
    case ActionType::KeyboardTap:
      return Tap(static_cast<const KeyboardTapAction&>(action));
    case ActionType::KeyboardPress:
      return Press(static_cast<const KeyboardPressAction&>(action));
    case ActionType::KeyboardRelease:
      return Release(static_cast<const KeyboardReleaseAction&>(action));
    case ActionType::KeyboardReset:
      return Reset();
    default:
      throw logic_error(format("{} is not a keyboard action.", action.Name()));
    }
  }

  value_ptr<ActionResult> KeyboardActionHandler::Type(const KeyboardTypeAction& action, const CancellationToken& cancellation)
  {
    auto pause = action.Pause ? milliseconds(*action.Pause) : _defaultTypingPause;
    for (auto character : ToCodePoints(action.Text))
    {
      //Line breaks and tabs as keys, as Unicode input does not press Enter or Tab in every application
      switch (character)
      {
      case U'\r':
        continue;
      case U'\n':
        TapKey(VK_RETURN);
        break;
      case U'\t':
        TapKey(VK_TAB);
        break;
      default:
        _keyboard->TypeCharacter(character);
        break;
      }

      if (cancellation.WaitFor(pause)) return ActionResult::Cancelled();
    }
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> KeyboardActionHandler::Tap(const KeyboardTapAction& action)
  {
    PressWithModifiers(action.Key, action.Modifiers);
    ReleaseWithModifiers(action.Key, action.Modifiers);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> KeyboardActionHandler::Press(const KeyboardPressAction& action)
  {
    PressWithModifiers(action.Key, action.Modifiers);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> KeyboardActionHandler::Release(const KeyboardReleaseAction& action)
  {
    ReleaseWithModifiers(action.Key, action.Modifiers);
    return ActionResult::Succeeded();
  }

  value_ptr<ActionResult> KeyboardActionHandler::Reset()
  {
    _inputState->ReleaseKeys();
    return ActionResult::Succeeded();
  }

  void KeyboardActionHandler::TapKey(uint16_t virtualKey)
  {
    _keyboard->Press(virtualKey);
    _keyboard->Release(virtualKey);
  }

  void KeyboardActionHandler::PressWithModifiers(uint16_t virtualKey, span<const KeyModifier> modifiers)
  {
    for (auto modifier : modifiers) _inputState->Press(VirtualKey(modifier));
    _inputState->Press(virtualKey);
  }

  void KeyboardActionHandler::ReleaseWithModifiers(uint16_t virtualKey, span<const KeyModifier> modifiers)
  {
    _inputState->Release(virtualKey);
    for (auto modifier : modifiers | views::reverse) _inputState->Release(VirtualKey(modifier));
  }
}
