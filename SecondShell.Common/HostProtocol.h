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

  //Whether input reaches the session. Windows stops all input to it while the user's own session is locked
  //(Findings.md 2.17).
  named_enum(InputAvailability,
    Available,
    Resuming,    //the user's session locked, and SecondShell is keeping it unlocked behind the lock screen: seconds
    Unavailable  //the user's session is locked: until the user signs in again
  );

  //Who controls the session (Design.md, Visualization).
  named_enum(ControlMode,
    Agent,    //the agent may use the session
    Stopped,  //the user stopped the agent
    User      //the user has taken control: their input goes to the session
  );

  //What changed in the session and around it, which the agent hears about with its next call.
  named_enum(SessionEventKind,
    ControlTaken,      //the user took control
    AgentStopped,      //the user stopped the agent
    ControlReturned,   //the user gave control back
    InputPaused,       //the user locked the PC, and SecondShell is keeping the session unlocked behind the lock screen
    InputUnavailable,  //the user's session is locked: no input until it is unlocked
    InputAvailable,
    UserLeft,          //the user's session is no longer unlocked on the PC's screen
    UserReturned
  );

  struct SECONDSHELL_COMMON_API SessionEvent
  {
    static Axodox::Json::json_object_descriptor<SessionEvent> json_description;

    std::string Time;  //local time, e.g. "2026-11-04T14:02:10+01:00"
    SessionEventKind Event = SessionEventKind::ControlTaken;
    std::string Message;
  };

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

    bool operator==(const DisplaySettings&) const = default;
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

    //Empty when SecondShell is not running
    std::optional<bool> IsUserPresent;          //the user's own session is unlocked on the PC's screen; false: unattended
    std::optional<InputAvailability> Input;
    std::optional<ControlMode> Control;
  };
#pragma endregion

#pragma region Session
  struct SECONDSHELL_COMMON_API SessionResponse
  {
    static Axodox::Json::json_object_descriptor<SessionResponse> json_description;

    SessionInfo Session;

    //Since the agent's previous call
    std::vector<SessionEvent> Events;
    std::vector<Actions::WindowInfo> NewWindows;
  };

  struct SECONDSHELL_COMMON_API InitializeSessionRequest
  {
    static Axodox::Json::json_object_descriptor<InitializeSessionRequest> json_description;
    using ResponseType = SessionResponse;

    //Sign-in and the driver's start: the driver starts with the user's startup programs, after the machine's, which
    //can take more than a minute (Findings.md 2.16)
    static constexpr std::chrono::milliseconds DefaultTimeout{ 180000 };
    //Without a timeout the host waits as long as the user's setting says, at most this long (SettingsStore)
    static constexpr std::chrono::milliseconds LongestTimeout{ 3600000 };

    std::optional<DisplaySettings> Display;
    std::optional<CaptureSettings> Capture;
    std::optional<ExistingSessionPolicy> ExistingSession;
    std::optional<uint32_t> Timeout;
  };

  struct SECONDSHELL_COMMON_API CheckSessionRequest
  {
    static Axodox::Json::json_object_descriptor<CheckSessionRequest> json_description;
    using ResponseType = SessionResponse;

    std::optional<uint32_t> WaitForControl;  //milliseconds to wait at most until the agent may send input
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

    //Since the agent's previous call
    std::vector<SessionEvent> Events;
    std::vector<Actions::WindowInfo> NewWindows;
  };

  //The arguments of the Run-Sequence tool.
  struct SECONDSHELL_COMMON_API RunSequenceArguments
  {
    static Axodox::Json::json_object_descriptor<RunSequenceArguments> json_description;
    static constexpr std::chrono::milliseconds DefaultTimeout{ 60000 };

    std::vector<Axodox::Infrastructure::value_ptr<Actions::Action>> Steps;
    std::optional<uint32_t> Timeout;
    std::optional<std::string> Description;
  };

  //The tool's arguments, and a progress token, which the AI does not set.
  struct SECONDSHELL_COMMON_API RunSequenceRequest : public RunSequenceArguments
  {
    static Axodox::Json::json_object_descriptor<RunSequenceRequest> json_description;
    using ResponseType = RunSequenceResponse;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ProgressToken;  //echoed in sequence/progress
  };

  //Host to MCP server: a step of a running sequence finished, or a long step is still running (when nothing has been
  //reported for a while).
  struct SECONDSHELL_COMMON_API SequenceProgressNotification
  {
    static Axodox::Json::json_object_descriptor<SequenceProgressNotification> json_description;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ProgressToken;
    uint32_t CompletedSteps = 0;
    uint32_t TotalSteps = 0;
    std::string Message;
    std::optional<double> StepProgress;  //of the running step, 0..1: how much of its time it has used
  };
#pragma endregion

#pragma region Display and capture
  struct SECONDSHELL_COMMON_API DisplayResponse
  {
    static Axodox::Json::json_object_descriptor<DisplayResponse> json_description;

    DisplaySettings Display;
  };

  //Display settings to change; what is not set stays as it is.
  struct SECONDSHELL_COMMON_API DisplayChange
  {
    static Axodox::Json::json_object_descriptor<DisplayChange> json_description;

    std::optional<uint32_t> Width;
    std::optional<uint32_t> Height;
    std::optional<uint32_t> Scale;
    std::optional<DisplayOrientation> Orientation;

    DisplaySettings ApplyTo(DisplaySettings settings) const;
  };

  struct SECONDSHELL_COMMON_API SetDisplayRequest
  {
    static Axodox::Json::json_object_descriptor<SetDisplayRequest> json_description;
    using ResponseType = DisplayResponse;

    DisplayChange Display;
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
