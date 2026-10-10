#pragma once
#include "Action.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API TouchPressAction : public TypedAction<ActionKind::Touch, ActionType::TouchPress>
  {
    static Axodox::Json::json_object_descriptor<TouchPressAction> json_description;

    uint32_t Id = 0;
    int32_t X = 0;
    int32_t Y = 0;
  };

  struct SECONDSHELL_COMMON_API TouchReleaseAction : public TypedAction<ActionKind::Touch, ActionType::TouchRelease>
  {
    static Axodox::Json::json_object_descriptor<TouchReleaseAction> json_description;

    uint32_t Id = 0;
  };

  struct SECONDSHELL_COMMON_API TouchTapAction : public TypedAction<ActionKind::Touch, ActionType::TouchTap>
  {
    static Axodox::Json::json_object_descriptor<TouchTapAction> json_description;

    uint32_t Id = 0;
    int32_t X = 0;
    int32_t Y = 0;
  };

  struct SECONDSHELL_COMMON_API TouchMoveAction : public TypedAction<ActionKind::Touch, ActionType::TouchMove>
  {
    static Axodox::Json::json_object_descriptor<TouchMoveAction> json_description;

    uint32_t Id = 0;
    int32_t X = 0;
    int32_t Y = 0;
    std::optional<uint32_t> Duration;
    std::optional<bool> Async;

    virtual bool IsAsync() const override;
  };

  struct SECONDSHELL_COMMON_API TouchResetAction : public TypedAction<ActionKind::Touch, ActionType::TouchReset>
  {
    static Axodox::Json::json_object_descriptor<TouchResetAction> json_description;
  };
}
