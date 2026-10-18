#pragma once
#include "Action.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API PenMoveAction : public TypedAction<ActionKind::Pen, ActionType::PenMove>
  {
    static Axodox::Json::json_object_descriptor<PenMoveAction> json_description;

    int32_t X = 0;
    int32_t Y = 0;
    std::optional<uint32_t> Duration;
    std::optional<bool> Async;
    std::optional<float> Pressure;
    std::optional<int32_t> TiltX;
    std::optional<int32_t> TiltY;
    std::optional<uint32_t> Rotation;

    virtual bool IsAsync() const override;
  };

  struct SECONDSHELL_COMMON_API PenPressAction : public TypedAction<ActionKind::Pen, ActionType::PenPress>
  {
    static Axodox::Json::json_object_descriptor<PenPressAction> json_description;

    std::optional<int32_t> X;
    std::optional<int32_t> Y;
    std::optional<float> Pressure;
    std::optional<int32_t> TiltX;
    std::optional<int32_t> TiltY;
    std::optional<uint32_t> Rotation;
    std::optional<bool> Barrel;
    std::optional<bool> Eraser;
  };

  struct SECONDSHELL_COMMON_API PenReleaseAction : public TypedAction<ActionKind::Pen, ActionType::PenRelease>
  {
    static Axodox::Json::json_object_descriptor<PenReleaseAction> json_description;
  };

  struct SECONDSHELL_COMMON_API PenTapAction : public TypedAction<ActionKind::Pen, ActionType::PenTap>
  {
    static Axodox::Json::json_object_descriptor<PenTapAction> json_description;

    int32_t X = 0;
    int32_t Y = 0;
    std::optional<float> Pressure;
    std::optional<bool> Barrel;
    std::optional<bool> Eraser;
  };

  struct SECONDSHELL_COMMON_API PenResetAction : public TypedAction<ActionKind::Pen, ActionType::PenReset>
  {
    static Axodox::Json::json_object_descriptor<PenResetAction> json_description;
  };
}
