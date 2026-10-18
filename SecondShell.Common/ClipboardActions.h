#pragma once
#include "Action.h"
#include "ActionValues.h"

namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API ClipboardReadAction : public TypedAction<ActionKind::Clipboard, ActionType::ClipboardRead>
  {
    static Axodox::Json::json_object_descriptor<ClipboardReadAction> json_description;

    std::optional<ClipboardFormat> Format;
    std::optional<std::string> Name;
    std::optional<CaptureOutput> Output;
    std::optional<std::string> Path;
    std::optional<Size> Resolution;
  };

  struct SECONDSHELL_COMMON_API ClipboardWriteAction : public TypedAction<ActionKind::Clipboard, ActionType::ClipboardWrite>
  {
    static Axodox::Json::json_object_descriptor<ClipboardWriteAction> json_description;

    std::vector<ClipboardContent> Content;
  };
}
