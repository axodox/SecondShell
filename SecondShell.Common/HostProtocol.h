#pragma once
#include "Action.h"
#include "ActionResult.h"
#include "EmptyResponse.h"

//The messages between the MCP server and the host, one request per MCP tool. Each request's descriptor name is its
//JSON-RPC method.
namespace SecondShell::Protocol
{
#pragma region Session values
  named_enum(SessionState,
    Stopped,
    Connecting,
    SigningIn,
    StartingDriver,
    Ready,
    Stopping,
    Failed
  );

  //What to do with a child session left over from before, e.g. after a host crash.
  named_enum(ExistingSessionPolicy,
    Resume,
    Reset
  );

  named_enum(DisplayOrientation,
    Landscape,
    Portrait,
    LandscapeFlipped,
    PortraitFlipped
  );

  struct SECONDSHELL_COMMON_API DisplaySettings
  {
    static Axodox::Json::json_object_descriptor<DisplaySettings> json_description;

    uint32_t Width = 1920;
    uint32_t Height = 1080;
    uint32_t Scale = 100;  //percent
    DisplayOrientation Orientation = DisplayOrientation::Landscape;
  };

  struct SECONDSHELL_COMMON_API CaptureSettings
  {
    static Axodox::Json::json_object_descriptor<CaptureSettings> json_description;

    std::optional<Actions::Size> Resolution;  //empty: full session resolution
    Actions::ImageFormat Format = Actions::ImageFormat::Png;
    uint32_t InlineLimit = 4;
  };

  struct SECONDSHELL_COMMON_API SessionInfo
  {
    static Axodox::Json::json_object_descriptor<SessionInfo> json_description;

    SessionState State = SessionState::Stopped;
    std::optional<uint32_t> SessionId;
    std::optional<DisplaySettings> Display;
    bool IsDriverConnected = false;
    std::optional<uint32_t> DriverProcessId;
  };
#pragma endregion

#pragma region Session
  struct SECONDSHELL_COMMON_API SessionResponse
  {
    static Axodox::Json::json_object_descriptor<SessionResponse> json_description;

    SessionInfo Session;
  };

  struct SECONDSHELL_COMMON_API InitializeSessionRequest
  {
    static Axodox::Json::json_object_descriptor<InitializeSessionRequest> json_description;
    using ResponseType = SessionResponse;
    static constexpr std::chrono::milliseconds DefaultTimeout{ 60000 };

    std::optional<DisplaySettings> Display;
    std::optional<CaptureSettings> Capture;
    std::optional<ExistingSessionPolicy> ExistingSession;
    std::optional<uint32_t> Timeout;
  };

  struct SECONDSHELL_COMMON_API CheckSessionRequest
  {
    static Axodox::Json::json_object_descriptor<CheckSessionRequest> json_description;
    using ResponseType = SessionResponse;
  };

  struct SECONDSHELL_COMMON_API ShutdownSessionRequest
  {
    static Axodox::Json::json_object_descriptor<ShutdownSessionRequest> json_description;
    using ResponseType = EmptyResponse;
    static constexpr std::chrono::milliseconds DefaultTimeout{ 30000 };

    std::optional<uint32_t> Timeout;
  };
#pragma endregion

#pragma region Sequence
  struct SECONDSHELL_COMMON_API RunSequenceResponse
  {
    static Axodox::Json::json_object_descriptor<RunSequenceResponse> json_description;

    std::vector<Axodox::Infrastructure::value_ptr<Actions::ActionResult>> Results;  //of the completed steps
    std::optional<uint32_t> FailedStep;                                               //index of the step which failed
    std::optional<Actions::ErrorCode> Error;
    std::optional<std::string> Message;
    std::optional<Actions::InputState> ReleasedInput;                                 //what was held when the sequence stopped early
  };

  //The arguments of the Run-Sequence tool.
  struct SECONDSHELL_COMMON_API RunSequenceArguments
  {
    static Axodox::Json::json_object_descriptor<RunSequenceArguments> json_description;
    static constexpr std::chrono::milliseconds DefaultTimeout{ 60000 };

    std::vector<Axodox::Infrastructure::value_ptr<Actions::Action>> Steps;
    std::optional<uint32_t> Timeout;
  };

  //The tool's arguments, and a progress token, which the AI does not set.
  struct SECONDSHELL_COMMON_API RunSequenceRequest : public RunSequenceArguments
  {
    static Axodox::Json::json_object_descriptor<RunSequenceRequest> json_description;
    using ResponseType = RunSequenceResponse;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ProgressToken;  //echoed in sequence/progress
  };

  //Host to MCP server: a step of a running sequence finished.
  struct SECONDSHELL_COMMON_API SequenceProgressNotification
  {
    static Axodox::Json::json_object_descriptor<SequenceProgressNotification> json_description;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ProgressToken;
    uint32_t CompletedSteps = 0;
    uint32_t TotalSteps = 0;
    std::string Message;
  };
#pragma endregion

#pragma region Display and capture
  struct SECONDSHELL_COMMON_API DisplayResponse
  {
    static Axodox::Json::json_object_descriptor<DisplayResponse> json_description;

    DisplaySettings Display;
  };

  struct SECONDSHELL_COMMON_API SetDisplayRequest
  {
    static Axodox::Json::json_object_descriptor<SetDisplayRequest> json_description;
    using ResponseType = DisplayResponse;

    DisplaySettings Display;
  };

  struct SECONDSHELL_COMMON_API GetDisplayRequest
  {
    static Axodox::Json::json_object_descriptor<GetDisplayRequest> json_description;
    using ResponseType = DisplayResponse;
  };

  struct SECONDSHELL_COMMON_API CaptureSettingsResponse
  {
    static Axodox::Json::json_object_descriptor<CaptureSettingsResponse> json_description;

    CaptureSettings Capture;
  };

  struct SECONDSHELL_COMMON_API ConfigureCaptureRequest
  {
    static Axodox::Json::json_object_descriptor<ConfigureCaptureRequest> json_description;
    using ResponseType = CaptureSettingsResponse;

    CaptureSettings Capture;
  };
#pragma endregion
}
