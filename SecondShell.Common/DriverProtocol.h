#pragma once
#include "Action.h"
#include "ActionResult.h"
#include "EmptyResponse.h"

//The messages between the host and the driver. Each request's descriptor name is its JSON-RPC method.
namespace SecondShell::Protocol
{
#pragma region driver/hello
  struct SECONDSHELL_COMMON_API DriverHelloResponse
  {
    static Axodox::Json::json_object_descriptor<DriverHelloResponse> json_description;

    bool IsAccepted = false;
    std::string Reason;
  };

  //Driver to host, first: the session guard. The driver does nothing else until it is accepted.
  struct SECONDSHELL_COMMON_API DriverHelloRequest
  {
    static Axodox::Json::json_object_descriptor<DriverHelloRequest> json_description;
    using ResponseType = DriverHelloResponse;

    std::string Version;
  };
#pragma endregion

#pragma region actions/execute
  struct SECONDSHELL_COMMON_API ExecuteActionResponse
  {
    static Axodox::Json::json_object_descriptor<ExecuteActionResponse> json_description;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Result;
  };

  //Host to driver: one action. An async action is answered when it has started, and reports its result later
  //with actions/completed under the same ID.
  struct SECONDSHELL_COMMON_API ExecuteActionRequest
  {
    static Axodox::Json::json_object_descriptor<ExecuteActionRequest> json_description;
    using ResponseType = ExecuteActionResponse;

    uint64_t ActionId = 0;
    Axodox::Infrastructure::value_ptr<Actions::Action> Action;
  };

  //Driver to host: an async action finished.
  struct SECONDSHELL_COMMON_API ActionCompletedNotification
  {
    static Axodox::Json::json_object_descriptor<ActionCompletedNotification> json_description;

    uint64_t ActionId = 0;
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Result;
  };
#pragma endregion

#pragma region input/reset
  struct SECONDSHELL_COMMON_API ResetInputResponse
  {
    static Axodox::Json::json_object_descriptor<ResetInputResponse> json_description;

    Actions::InputState Released;
  };

  //Host to driver: release all held input (Control.Reset, Stop, failures).
  struct SECONDSHELL_COMMON_API ResetInputRequest
  {
    static Axodox::Json::json_object_descriptor<ResetInputRequest> json_description;
    using ResponseType = ResetInputResponse;
  };
#pragma endregion

#pragma region screen/get
  struct SECONDSHELL_COMMON_API ScreenResponse
  {
    static Axodox::Json::json_object_descriptor<ScreenResponse> json_description;

    uint32_t Width = 0;
    uint32_t Height = 0;
  };

  //Host to driver: the size of the session's screen, as the driver sees it, e.g. once a display change has applied.
  struct SECONDSHELL_COMMON_API GetScreenRequest
  {
    static Axodox::Json::json_object_descriptor<GetScreenRequest> json_description;
    using ResponseType = ScreenResponse;
  };
#pragma endregion

#pragma region frames/subscribe
  struct SECONDSHELL_COMMON_API SubscribeFramesResponse
  {
    static Axodox::Json::json_object_descriptor<SubscribeFramesResponse> json_description;

    std::vector<uint64_t> Textures;  //NT handles, valid in the host process
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint64_t AdapterLuid = 0;        //the host creates its device on this adapter
  };

  //Host to driver: share the captured frames with the host process (Findings.md 2.13).
  struct SECONDSHELL_COMMON_API SubscribeFramesRequest
  {
    static Axodox::Json::json_object_descriptor<SubscribeFramesRequest> json_description;
    using ResponseType = SubscribeFramesResponse;

    uint32_t HostProcessId = 0;
  };
#pragma endregion

#pragma region Interactive mode
  //Host to driver: start or stop watching the session's clipboard for clipboard/changed.
  struct SECONDSHELL_COMMON_API SetClipboardSyncRequest
  {
    static Axodox::Json::json_object_descriptor<SetClipboardSyncRequest> json_description;
    using ResponseType = EmptyResponse;

    bool IsEnabled = false;
  };

  //Host to driver: put the user's clipboard content on the session's clipboard.
  struct SECONDSHELL_COMMON_API SetClipboardRequest
  {
    static Axodox::Json::json_object_descriptor<SetClipboardRequest> json_description;
    using ResponseType = EmptyResponse;

    std::vector<Actions::ClipboardContent> Contents;
  };

  //Driver to host: the session's clipboard changed.
  struct SECONDSHELL_COMMON_API ClipboardChangedNotification
  {
    static Axodox::Json::json_object_descriptor<ClipboardChangedNotification> json_description;

    std::vector<Actions::ClipboardContent> Contents;
  };

  //Host to driver: the user's input in interactive mode, as input actions (Mouse.Move, Keyboard.Press, ...).
  struct SECONDSHELL_COMMON_API ForwardUserInputNotification
  {
    static Axodox::Json::json_object_descriptor<ForwardUserInputNotification> json_description;

    std::vector<Axodox::Infrastructure::value_ptr<Actions::Action>> Inputs;
  };
#pragma endregion
}
