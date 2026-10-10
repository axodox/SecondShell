#include "pch.h"
#include "HostProtocol.h"

using namespace Axodox::Json;

namespace SecondShell::Protocol
{
  namespace
  {
    const char* const _timeout = "Milliseconds to wait at most; when it expires the tool returns a Timeout error.";
  }

  json_object_descriptor<DisplaySettings> DisplaySettings::json_description = describe_json_object<DisplaySettings>({
    { &DisplaySettings::Width, "width", { .schema = { .description = "Session pixels.", .minimum = 640, .maximum = 8192 } } },
    { &DisplaySettings::Height, "height", { .schema = { .description = "Session pixels.", .minimum = 480, .maximum = 8192 } } },
    { &DisplaySettings::Scale, "scale", { .schema = { .description = "Display scaling in percent.", .minimum = 100, .maximum = 500 } } },
    { &DisplaySettings::Orientation, "orientation" }
    });

  json_object_descriptor<CaptureSettings> CaptureSettings::json_description = describe_json_object<CaptureSettings>({
    { &CaptureSettings::Resolution, "resolution", { .is_required = false, .schema = { .description = "Largest capture size: captures are scaled down to fit, never up. Default: full session resolution." } } },
    { &CaptureSettings::Format, "format", { .schema = { .description = "Default image format of captures saved to file: Png or Jpeg." } } },
    { &CaptureSettings::InlineLimit, "inlineLimit", { .schema = { .description = "Most inline captures per sequence; further ones are saved to file.", .minimum = 0 } } }
    });

  json_object_descriptor<SessionInfo> SessionInfo::json_description = describe_json_object<SessionInfo>({
    { &SessionInfo::State, "state" },
    { &SessionInfo::SessionId, "sessionId", { .is_required = false } },
    { &SessionInfo::Display, "display", { .is_required = false } },
    { &SessionInfo::IsDriverConnected, "isDriverConnected" },
    { &SessionInfo::DriverProcessId, "driverPid", { .is_required = false } }
    });

  json_object_descriptor<SessionResponse> SessionResponse::json_description = describe_json_object<SessionResponse>({
    { &SessionResponse::Session, "session" }
    });

  json_object_descriptor<InitializeSessionRequest> InitializeSessionRequest::json_description = describe_json_object<InitializeSessionRequest>(
    { .name = "session/initialize", .description = "Starts the session, a second desktop of the same user, and waits until it is ready." },
    {
      { &InitializeSessionRequest::Display, "display", { .is_required = false, .schema = { .description = "Display settings; default: 1920x1080 at 100%." } } },
      { &InitializeSessionRequest::Capture, "capture", { .is_required = false, .schema = { .description = "Capture defaults, as Configure-Capture sets them." } } },
      { &InitializeSessionRequest::ExistingSession, "existingSession", { .is_required = false, .schema = { .description = "If a session is left over from before: Resume it with its apps, or Reset it (log off and start a new one)." } } },
      { &InitializeSessionRequest::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds to wait for the session, default 60000.", .minimum = 0 } } }
    });

  json_object_descriptor<CheckSessionRequest> CheckSessionRequest::json_description = describe_json_object<CheckSessionRequest>(
    { .name = "session/check", .description = "Returns the state of the session: whether it is ready, its display settings and the driver's process ID." }, {});

  json_object_descriptor<ShutdownSessionRequest> ShutdownSessionRequest::json_description = describe_json_object<ShutdownSessionRequest>(
    { .name = "session/shutdown", .description = "Logs the session off, closing all its applications." },
    {
      { &ShutdownSessionRequest::Timeout, "timeout", { .is_required = false, .schema = { .description = _timeout, .minimum = 0 } } }
    });

  json_object_descriptor<RunSequenceResponse> RunSequenceResponse::json_description = describe_json_object<RunSequenceResponse>({
    { &RunSequenceResponse::Results, "results" },
    { &RunSequenceResponse::FailedStep, "failedStep", { .is_required = false } },
    { &RunSequenceResponse::Error, "error", { .is_required = false } },
    { &RunSequenceResponse::Message, "message", { .is_required = false } },
    { &RunSequenceResponse::ReleasedInput, "releasedInput", { .is_required = false } }
    });

  json_object_descriptor<RunSequenceRequest> RunSequenceRequest::json_description = describe_json_object<RunSequenceRequest>(
    { .name = "sequence/run", .description = "Runs actions in order in the session, until one fails: input, captures, windows, processes, clipboard. Captures are returned as images." },
    {
      { &RunSequenceRequest::Steps, "steps", { .is_required = true, .schema = { .description = "The actions, run in order. Coordinates are session pixels.", .min_items = 1 } } },
      { &RunSequenceRequest::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds for the whole sequence, default 60000.", .minimum = 0 } } },
      { &RunSequenceRequest::ProgressToken, "progressToken", { .is_required = false } }
    });

  json_object_descriptor<SequenceProgressNotification> SequenceProgressNotification::json_description = describe_json_object<SequenceProgressNotification>(
    { .name = "sequence/progress" },
    {
      { &SequenceProgressNotification::ProgressToken, "progressToken", { .is_required = false } },
      { &SequenceProgressNotification::CompletedSteps, "completedSteps" },
      { &SequenceProgressNotification::TotalSteps, "totalSteps" },
      { &SequenceProgressNotification::Message, "message", { .is_required = false } }
    });

  json_object_descriptor<DisplayResponse> DisplayResponse::json_description = describe_json_object<DisplayResponse>({
    { &DisplayResponse::Display, "display" }
    });

  json_object_descriptor<SetDisplayRequest> SetDisplayRequest::json_description = describe_json_object<SetDisplayRequest>(
    { .name = "display/set", .description = "Changes the session's display resolution, scaling and orientation." },
    {
      { &SetDisplayRequest::Display, "display", { .is_required = true } }
    });

  json_object_descriptor<GetDisplayRequest> GetDisplayRequest::json_description = describe_json_object<GetDisplayRequest>(
    { .name = "display/get", .description = "Returns the session's display settings." }, {});

  json_object_descriptor<CaptureSettingsResponse> CaptureSettingsResponse::json_description = describe_json_object<CaptureSettingsResponse>({
    { &CaptureSettingsResponse::Capture, "capture" }
    });

  json_object_descriptor<ConfigureCaptureRequest> ConfigureCaptureRequest::json_description = describe_json_object<ConfigureCaptureRequest>(
    { .name = "capture/configure", .description = "Sets the session's capture defaults: resolution, format and the most inline captures per sequence. Capture actions may override resolution and format." },
    {
      { &ConfigureCaptureRequest::Capture, "capture", { .is_required = true } }
    });
}
