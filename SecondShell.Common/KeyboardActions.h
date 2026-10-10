#pragma once
#include "Action.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API KeyboardTypeAction : public TypedAction<ActionKind::Keyboard, ActionType::KeyboardType>
  {
    static Axodox::Json::json_object_descriptor<KeyboardTypeAction> json_description;

    std::string Text;
    std::optional<uint32_t> Pause;
    std::optional<bool> Sensitive;
  };

  struct SECONDSHELL_COMMON_API KeyboardTapAction : public TypedAction<ActionKind::Keyboard, ActionType::KeyboardTap>
  {
    static Axodox::Json::json_object_descriptor<KeyboardTapAction> json_description;

    uint16_t Key = 0;  //virtual key code
    std::vector<KeyModifier> Modifiers;
  };

  struct SECONDSHELL_COMMON_API KeyboardPressAction : public TypedAction<ActionKind::Keyboard, ActionType::KeyboardPress>
  {
    static Axodox::Json::json_object_descriptor<KeyboardPressAction> json_description;

    uint16_t Key = 0;
    std::vector<KeyModifier> Modifiers;
  };

  struct SECONDSHELL_COMMON_API KeyboardReleaseAction : public TypedAction<ActionKind::Keyboard, ActionType::KeyboardRelease>
  {
    static Axodox::Json::json_object_descriptor<KeyboardReleaseAction> json_description;

    uint16_t Key = 0;
    std::vector<KeyModifier> Modifiers;
  };

  struct SECONDSHELL_COMMON_API KeyboardResetAction : public TypedAction<ActionKind::Keyboard, ActionType::KeyboardReset>
  {
    static Axodox::Json::json_object_descriptor<KeyboardResetAction> json_description;
  };
}
