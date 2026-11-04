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

#pragma region frames/subscribe, frames/changed
  //A ring of shared frame textures (SharedFrames.h), as handles in the host process, which owns them.
  struct SECONDSHELL_COMMON_API SharedFrames
  {
    static Axodox::Json::json_object_descriptor<SharedFrames> json_description;

    std::vector<uint64_t> Textures;  //NT handles of the textures
    uint64_t State = 0;              //the section of the SharedFrameState
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint64_t AdapterLuid = 0;        //the host opens the textures on a device of this adapter
  };

  struct SECONDSHELL_COMMON_API SubscribeFramesResponse
  {
    static Axodox::Json::json_object_descriptor<SubscribeFramesResponse> json_description;

    SharedFrames Frames;
  };

  //Host to driver: share the captured frames with the host, the process at the other end of the pipe (Findings.md 2.13).
  struct SECONDSHELL_COMMON_API SubscribeFramesRequest
  {
    static Axodox::Json::json_object_descriptor<SubscribeFramesRequest> json_description;
    using ResponseType = SubscribeFramesResponse;
  };

  //Driver to host: the frames are in a new ring, e.g. after a resolution change; the previous one is not written any more.
  struct SECONDSHELL_COMMON_API FramesChangedNotification
  {
    static Axodox::Json::json_object_descriptor<FramesChangedNotification> json_description;

    SharedFrames Frames;
  };
#pragma endregion

#pragma region Process output
  //Driver to host: a process started with output began, wrote more, or ended; a few times a second while it writes.
  //The text follows on from the previous notification's, unless the host's view fell behind what the driver keeps.
  struct SECONDSHELL_COMMON_API ProcessOutputNotification
  {
    static Axodox::Json::json_object_descriptor<ProcessOutputNotification> json_description;

    uint32_t ProcessId = 0;
    std::string Command;
    std::string OutputFile;
    uint64_t Offset = 0;            //of the text in the output
    std::string Text;
    bool IsRunning = true;
    std::optional<int32_t> ExitCode;
  };

  //Host to driver: how the output of processes is kept, from the user's settings; at the start and on changes.
  struct SECONDSHELL_COMMON_API ConfigureOutputNotification
  {
    static Axodox::Json::json_object_descriptor<ConfigureOutputNotification> json_description;

    uint32_t PartSize = 100;  //megabytes per file, then the output continues in the next one
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
