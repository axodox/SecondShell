#pragma once
#include "Action.h"
#include "ActionValues.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API WindowEnumerateAction : public TypedAction<ActionKind::Window, ActionType::WindowEnumerate>
  {
    static Axodox::Json::json_object_descriptor<WindowEnumerateAction> json_description;

    std::optional<bool> All;  //also dialogs and other windows the taskbar does not show, if they have a title
  };

  //An action on one window, which it finds by ID, or by title, process name or process ID: IDs change when an
  //application restarts.
  struct SECONDSHELL_COMMON_API WindowTargetAction : public Action
  {
    static Axodox::Json::json_object_descriptor<WindowTargetAction> json_description;

    std::optional<WindowId> Window;
    std::optional<std::string> Title;
    std::optional<std::string> ProcessName;
    std::optional<uint32_t> ProcessId;

    WindowQuery Query() const;
    virtual Axodox::Infrastructure::expected<> Validate() const override;
  };

  struct SECONDSHELL_COMMON_API WindowFocusAction : public TypedAction<ActionKind::Window, ActionType::WindowFocus, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowFocusAction> json_description;
  };

  struct SECONDSHELL_COMMON_API WindowMoveAction : public TypedAction<ActionKind::Window, ActionType::WindowMove, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowMoveAction> json_description;

    int32_t X = 0;
    int32_t Y = 0;
  };

  struct SECONDSHELL_COMMON_API WindowResizeAction : public TypedAction<ActionKind::Window, ActionType::WindowResize, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowResizeAction> json_description;

    uint32_t Width = 0;
    uint32_t Height = 0;
  };

  struct SECONDSHELL_COMMON_API WindowMinimizeAction : public TypedAction<ActionKind::Window, ActionType::WindowMinimize, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowMinimizeAction> json_description;
  };

  struct SECONDSHELL_COMMON_API WindowMaximizeAction : public TypedAction<ActionKind::Window, ActionType::WindowMaximize, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowMaximizeAction> json_description;
  };

  struct SECONDSHELL_COMMON_API WindowRestoreAction : public TypedAction<ActionKind::Window, ActionType::WindowRestore, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowRestoreAction> json_description;
  };

  struct SECONDSHELL_COMMON_API WindowCloseAction : public TypedAction<ActionKind::Window, ActionType::WindowClose, WindowTargetAction>
  {
    static Axodox::Json::json_object_descriptor<WindowCloseAction> json_description;
  };
}
