#include "pch.h"
#include "HostProtocol.h"

using namespace Axodox::Json;

namespace SecondShell::Protocol
{
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
      { &InitializeSessionRequest::Display, "display", { .is_required = false, .schema = { .description = "Display settings; default: the user's setting in SecondShell, 1920x1080 at 100% unless they changed it." } } },
      { &InitializeSessionRequest::Capture, "capture", { .is_required = false, .schema = { .description = "Capture defaults, as Configure-Capture sets them." } } },
      { &InitializeSessionRequest::ExistingSession, "existingSession", { .is_required = false, .schema = { .description = "Required when a child session exists already, e.g. from before a restart: Reset logs it off, closing its applications, and starts a new one. Resume is not supported yet." } } },
      { &InitializeSessionRequest::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds to wait for the session; default: the user's sign-in timeout setting, 3 minutes unless they changed it.", .minimum = 0 } } }
    });

  json_object_descriptor<CheckSessionRequest> CheckSessionRequest::json_description = describe_json_object<CheckSessionRequest>(
    { .name = "session/check", .description = "Returns the state of the session: whether it is ready, its display settings and the driver's process ID." }, {});

  json_object_descriptor<ShutdownSessionRequest> ShutdownSessionRequest::json_description = describe_json_object<ShutdownSessionRequest>(
    { .name = "session/shutdown", .description = "Logs the session off, closing all its applications." },
    {
      { &ShutdownSessionRequest::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds to wait for the session to log off, default 30000.", .minimum = 0 } } }
    });

  json_object_descriptor<RunSequenceResponse> RunSequenceResponse::json_description = describe_json_object<RunSequenceResponse>({
    { &RunSequenceResponse::Results, "results" },
    { &RunSequenceResponse::FailedStep, "failedStep", { .is_required = false } },
    { &RunSequenceResponse::Error, "error", { .is_required = false } },
    { &RunSequenceResponse::Message, "message", { .is_required = false } },
    { &RunSequenceResponse::ReleasedInput, "releasedInput", { .is_required = false } }
    });

  json_object_descriptor<RunSequenceArguments> RunSequenceArguments::json_description = describe_json_object<RunSequenceArguments>(
    "Runs actions in order in the session, until one fails: input, captures, windows, processes, clipboard. Captures are returned as images.",
    {
      { &RunSequenceArguments::Steps, "steps", { .is_required = true, .schema = { .description = "The actions, run in order. Coordinates are session pixels.", .min_items = 1 } } },
      { &RunSequenceArguments::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds for the whole sequence, default 60000.", .minimum = 0 } } },
      { &RunSequenceArguments::Description, "description", { .is_required = false, .schema = { .description = "What the sequence does, in a few words, for the user watching SecondShell, e.g. \"Opening the settings\"." } } }
    });

  json_object_descriptor<RunSequenceRequest> RunSequenceRequest::json_description = describe_json_object<RunSequenceRequest, RunSequenceArguments>(
    { .name = "sequence/run" },
    {
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

  json_object_descriptor<DisplayChange> DisplayChange::json_description = describe_json_object<DisplayChange>({
    { &DisplayChange::Width, "width", { .is_required = false, .schema = { .description = "Session pixels.", .minimum = 640, .maximum = 8192 } } },
    { &DisplayChange::Height, "height", { .is_required = false, .schema = { .description = "Session pixels.", .minimum = 480, .maximum = 8192 } } },
    { &DisplayChange::Scale, "scale", { .is_required = false, .schema = { .description = "Display scaling in percent.", .minimum = 100, .maximum = 500 } } },
    { &DisplayChange::Orientation, "orientation", { .is_required = false } }
    });

  DisplaySettings DisplayChange::ApplyTo(DisplaySettings settings) const
  {
    if (Width) settings.Width = *Width;
    if (Height) settings.Height = *Height;
    if (Scale) settings.Scale = *Scale;
    if (Orientation) settings.Orientation = *Orientation;
    return settings;
  }

  json_object_descriptor<SetDisplayRequest> SetDisplayRequest::json_description = describe_json_object<SetDisplayRequest>(
    { .name = "display/set", .description = "Changes the session's display resolution, scaling and orientation; what is left out stays as it is." },
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
