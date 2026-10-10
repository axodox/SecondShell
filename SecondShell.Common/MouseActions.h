#pragma once
#include "Action.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API MouseMoveAction : public TypedAction<ActionKind::Mouse, ActionType::MouseMove>
  {
    static Axodox::Json::json_object_descriptor<MouseMoveAction> json_description;

    int32_t X = 0;
    int32_t Y = 0;
    std::optional<uint32_t> Duration;
    std::optional<bool> Async;

    virtual bool IsAsync() const override;
  };

  struct SECONDSHELL_COMMON_API MouseClickAction : public TypedAction<ActionKind::Mouse, ActionType::MouseClick>
  {
    static Axodox::Json::json_object_descriptor<MouseClickAction> json_description;

    MouseButton Button = MouseButton::Left;
    std::optional<int32_t> X;
    std::optional<int32_t> Y;
    std::optional<uint32_t> Duration;
    std::optional<bool> Async;

    virtual bool IsAsync() const override;
  };

  struct SECONDSHELL_COMMON_API MouseDoubleClickAction : public TypedAction<ActionKind::Mouse, ActionType::MouseDoubleClick>
  {
    static Axodox::Json::json_object_descriptor<MouseDoubleClickAction> json_description;

    MouseButton Button = MouseButton::Left;
    std::optional<int32_t> X;
    std::optional<int32_t> Y;
  };

  struct SECONDSHELL_COMMON_API MouseWheelAction : public TypedAction<ActionKind::Mouse, ActionType::MouseWheel>
  {
    static Axodox::Json::json_object_descriptor<MouseWheelAction> json_description;

    float Delta = 0.f;
    WheelAxis Axis = WheelAxis::Vertical;
    std::optional<int32_t> X;
    std::optional<int32_t> Y;
  };

  struct SECONDSHELL_COMMON_API MousePressAction : public TypedAction<ActionKind::Mouse, ActionType::MousePress>
  {
    static Axodox::Json::json_object_descriptor<MousePressAction> json_description;

    MouseButton Button = MouseButton::Left;
  };

  struct SECONDSHELL_COMMON_API MouseReleaseAction : public TypedAction<ActionKind::Mouse, ActionType::MouseRelease>
  {
    static Axodox::Json::json_object_descriptor<MouseReleaseAction> json_description;

    MouseButton Button = MouseButton::Left;
  };

  struct SECONDSHELL_COMMON_API MouseResetAction : public TypedAction<ActionKind::Mouse, ActionType::MouseReset>
  {
    static Axodox::Json::json_object_descriptor<MouseResetAction> json_description;
  };
}
