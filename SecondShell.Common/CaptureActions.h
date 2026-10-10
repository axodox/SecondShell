#pragma once
#include "Action.h"
#include "ActionValues.h"

namespace SecondShell::Actions
{
  //The output options shared by the actions which take a capture.
  struct SECONDSHELL_COMMON_API CaptureAction : public Action
  {
    static Axodox::Json::json_object_descriptor<CaptureAction> json_description;

    std::optional<std::string> Tag;
    std::optional<CaptureOutput> Output;
    std::optional<std::string> Path;
    std::optional<ImageFormat> Format;
    std::optional<Size> Resolution;
  };

  struct SECONDSHELL_COMMON_API CaptureScreenAction : public TypedAction<ActionKind::Capture, ActionType::CaptureScreen, CaptureAction>
  {
    static Axodox::Json::json_object_descriptor<CaptureScreenAction> json_description;
  };

  struct SECONDSHELL_COMMON_API CaptureWindowAction : public TypedAction<ActionKind::Capture, ActionType::CaptureWindow, CaptureAction>
  {
    static Axodox::Json::json_object_descriptor<CaptureWindowAction> json_description;

    std::optional<WindowId> Window;
  };

  struct SECONDSHELL_COMMON_API CaptureRegionAction : public TypedAction<ActionKind::Capture, ActionType::CaptureRegion, CaptureAction>
  {
    static Axodox::Json::json_object_descriptor<CaptureRegionAction> json_description;

    int32_t X = 0;
    int32_t Y = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
  };

  struct SECONDSHELL_COMMON_API CaptureWaitForTextAction : public TypedAction<ActionKind::Capture, ActionType::CaptureWaitForText>
  {
    static Axodox::Json::json_object_descriptor<CaptureWaitForTextAction> json_description;

    std::string Text;
    std::optional<uint32_t> Interval;
    std::optional<uint32_t> Timeout;

    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
  };

  struct SECONDSHELL_COMMON_API CaptureWaitForStableAction : public TypedAction<ActionKind::Capture, ActionType::CaptureWaitForStable>
  {
    static Axodox::Json::json_object_descriptor<CaptureWaitForStableAction> json_description;

    std::optional<uint32_t> Stable;
    std::optional<WindowId> Window;
    std::optional<int32_t> X;
    std::optional<int32_t> Y;
    std::optional<uint32_t> Width;
    std::optional<uint32_t> Height;
    std::optional<uint32_t> Interval;
    std::optional<uint32_t> Timeout;

    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const override;
  };
}
