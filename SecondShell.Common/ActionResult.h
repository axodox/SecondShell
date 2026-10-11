#pragma once
#include "ActionValues.h"

namespace SecondShell::Actions
{
  //The result of an action. Its type discriminator is "type"; actions which return data have a derived result.
  struct SECONDSHELL_COMMON_API ActionResult
  {
    static Axodox::Json::json_object_descriptor<ActionResult> json_description;

    ActionStatus Status = ActionStatus::Succeeded;
    std::optional<ErrorCode> Error;
    std::optional<std::string> Message;

    virtual ~ActionResult() = default;

    static Axodox::Infrastructure::value_ptr<ActionResult> Succeeded();
    static Axodox::Infrastructure::value_ptr<ActionResult> Cancelled();
    static Axodox::Infrastructure::value_ptr<ActionResult> Failed(ErrorCode error, std::string message);
  };

  struct SECONDSHELL_COMMON_API CaptureResult : public ActionResult
  {
    static Axodox::Json::json_object_descriptor<CaptureResult> json_description;

    std::string Tag;
    CapturedImage Image;
  };

  struct SECONDSHELL_COMMON_API ProcessResult : public ActionResult
  {
    static Axodox::Json::json_object_descriptor<ProcessResult> json_description;

    uint32_t ProcessId = 0;
    std::optional<int32_t> ExitCode;
    std::optional<std::string> StandardOutput;
    std::optional<std::string> StandardError;
    std::optional<bool> IsOutputTruncated;
    std::optional<bool> IsErrorTruncated;
  };

  struct SECONDSHELL_COMMON_API ProcessListResult : public ActionResult
  {
    static Axodox::Json::json_object_descriptor<ProcessListResult> json_description;

    std::vector<ProcessInfo> Processes;
  };

  struct SECONDSHELL_COMMON_API WindowListResult : public ActionResult
  {
    static Axodox::Json::json_object_descriptor<WindowListResult> json_description;

    std::vector<WindowInfo> Windows;
  };

  struct SECONDSHELL_COMMON_API ClipboardResult : public ActionResult
  {
    static Axodox::Json::json_object_descriptor<ClipboardResult> json_description;

    std::vector<std::string> Formats;
    std::optional<ClipboardContent> Content;
    std::optional<CapturedImage> Image;
  };

  //The schema of a result property: any of the results.
  struct SECONDSHELL_COMMON_API ActionResultSchema : public Axodox::Json::json_type_schema<ActionResultSchema, Axodox::Json::json_type::object>
  {
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> to_json() const;
  };
}

namespace Axodox::Json
{
  template<>
  struct json_type_metadata<Axodox::Infrastructure::value_ptr<SecondShell::Actions::ActionResult>>
  {
    using type = SecondShell::Actions::ActionResultSchema;
  };
}
