#pragma once
#include "Action.h"
#include "WindowId.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API WindowEnumerateAction : public TypedAction<ActionKind::Window, ActionType::WindowEnumerate>
  {
    static Axodox::Json::json_object_descriptor<WindowEnumerateAction> json_description;
  };

  struct SECONDSHELL_COMMON_API WindowFocusAction : public TypedAction<ActionKind::Window, ActionType::WindowFocus>
  {
    static Axodox::Json::json_object_descriptor<WindowFocusAction> json_description;

    WindowId Window;
  };

  struct SECONDSHELL_COMMON_API WindowMoveAction : public TypedAction<ActionKind::Window, ActionType::WindowMove>
  {
    static Axodox::Json::json_object_descriptor<WindowMoveAction> json_description;

    WindowId Window;
    int32_t X = 0;
    int32_t Y = 0;
  };

  struct SECONDSHELL_COMMON_API WindowResizeAction : public TypedAction<ActionKind::Window, ActionType::WindowResize>
  {
    static Axodox::Json::json_object_descriptor<WindowResizeAction> json_description;

    WindowId Window;
    uint32_t Width = 0;
    uint32_t Height = 0;
  };

  struct SECONDSHELL_COMMON_API WindowMinimizeAction : public TypedAction<ActionKind::Window, ActionType::WindowMinimize>
  {
    static Axodox::Json::json_object_descriptor<WindowMinimizeAction> json_description;

    WindowId Window;
  };

  struct SECONDSHELL_COMMON_API WindowMaximizeAction : public TypedAction<ActionKind::Window, ActionType::WindowMaximize>
  {
    static Axodox::Json::json_object_descriptor<WindowMaximizeAction> json_description;

    WindowId Window;
  };

  struct SECONDSHELL_COMMON_API WindowRestoreAction : public TypedAction<ActionKind::Window, ActionType::WindowRestore>
  {
    static Axodox::Json::json_object_descriptor<WindowRestoreAction> json_description;

    WindowId Window;
  };

  struct SECONDSHELL_COMMON_API WindowCloseAction : public TypedAction<ActionKind::Window, ActionType::WindowClose>
  {
    static Axodox::Json::json_object_descriptor<WindowCloseAction> json_description;

    WindowId Window;
  };
}
