#include "pch.h"
#include "DriverProtocol.h"

using namespace Axodox::Json;

namespace SecondShell::Protocol
{
  json_object_descriptor<DriverHelloResponse> DriverHelloResponse::json_description = describe_json_object<DriverHelloResponse>({
    { &DriverHelloResponse::IsAccepted, "isAccepted" },
    { &DriverHelloResponse::Reason, "reason", { .is_required = false } }
    });

  json_object_descriptor<DriverHelloRequest> DriverHelloRequest::json_description = describe_json_object<DriverHelloRequest>(
    { .name = "driver/hello" },
    {
      { &DriverHelloRequest::Version, "version", { .is_required = true } }
    });

  json_object_descriptor<ExecuteActionResponse> ExecuteActionResponse::json_description = describe_json_object<ExecuteActionResponse>({
    { &ExecuteActionResponse::Result, "result", { .is_required = true } }
    });

  json_object_descriptor<ExecuteActionRequest> ExecuteActionRequest::json_description = describe_json_object<ExecuteActionRequest>(
    { .name = "actions/execute" },
    {
      { &ExecuteActionRequest::ActionId, "actionId", { .is_required = true } },
      { &ExecuteActionRequest::Action, "action", { .is_required = true } }
    });

  json_object_descriptor<ActionCompletedNotification> ActionCompletedNotification::json_description = describe_json_object<ActionCompletedNotification>(
    { .name = "actions/completed" },
    {
      { &ActionCompletedNotification::ActionId, "actionId", { .is_required = true } },
      { &ActionCompletedNotification::Result, "result", { .is_required = true } }
    });

  json_object_descriptor<ResetInputResponse> ResetInputResponse::json_description = describe_json_object<ResetInputResponse>({
    { &ResetInputResponse::Released, "released" }
    });

  json_object_descriptor<ResetInputRequest> ResetInputRequest::json_description = describe_json_object<ResetInputRequest>({ .name = "input/reset" }, {});

  json_object_descriptor<SubscribeFramesResponse> SubscribeFramesResponse::json_description = describe_json_object<SubscribeFramesResponse>({
    { &SubscribeFramesResponse::Textures, "textures" },
    { &SubscribeFramesResponse::Width, "width" },
    { &SubscribeFramesResponse::Height, "height" },
    { &SubscribeFramesResponse::AdapterLuid, "adapterLuid" }
    });

  json_object_descriptor<SubscribeFramesRequest> SubscribeFramesRequest::json_description = describe_json_object<SubscribeFramesRequest>(
    { .name = "frames/subscribe" },
    {
      { &SubscribeFramesRequest::HostProcessId, "hostProcessId", { .is_required = true } }
    });

  json_object_descriptor<SetClipboardSyncRequest> SetClipboardSyncRequest::json_description = describe_json_object<SetClipboardSyncRequest>(
    { .name = "clipboard/sync" },
    {
      { &SetClipboardSyncRequest::IsEnabled, "isEnabled", { .is_required = true } }
    });

  json_object_descriptor<SetClipboardRequest> SetClipboardRequest::json_description = describe_json_object<SetClipboardRequest>(
    { .name = "clipboard/set" },
    {
      { &SetClipboardRequest::Contents, "contents", { .is_required = true } }
    });

  json_object_descriptor<ClipboardChangedNotification> ClipboardChangedNotification::json_description = describe_json_object<ClipboardChangedNotification>(
    { .name = "clipboard/changed" },
    {
      { &ClipboardChangedNotification::Contents, "contents", { .is_required = true } }
    });

  json_object_descriptor<ForwardUserInputNotification> ForwardUserInputNotification::json_description = describe_json_object<ForwardUserInputNotification>(
    { .name = "input/forward" },
    {
      { &ForwardUserInputNotification::Inputs, "inputs", { .is_required = true } }
    });
}
