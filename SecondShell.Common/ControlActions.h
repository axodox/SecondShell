#pragma once
#include "Action.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API ControlWaitAction : public TypedAction<ActionKind::Control, ActionType::ControlWait>
  {
    static Axodox::Json::json_object_descriptor<ControlWaitAction> json_description;

    uint32_t Duration = 0;
  };

  struct SECONDSHELL_COMMON_API ControlJoinAction : public TypedAction<ActionKind::Control, ActionType::ControlJoin>
  {
    static Axodox::Json::json_object_descriptor<ControlJoinAction> json_description;
  };

  struct SECONDSHELL_COMMON_API ControlResetAction : public TypedAction<ActionKind::Control, ActionType::ControlReset>
  {
    static Axodox::Json::json_object_descriptor<ControlResetAction> json_description;
  };
}
