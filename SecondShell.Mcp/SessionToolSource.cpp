#include "pch.h"
#include "SessionToolSource.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Mcp
{
  //How long the host may take for requests without a timeout, and beyond the timeout of those with one
  const milliseconds _hostTimeout{ 30000 };
  const milliseconds _hostMargin{ 10000 };

  namespace
  {
    milliseconds HostTimeout(optional<uint32_t> timeout, milliseconds defaultTimeout)
    {
      return (timeout ? milliseconds(*timeout) : defaultTimeout) + _hostMargin;
    }
  }

  SessionToolSource::SessionToolSource(dependency_container* container) :
    _host(container->resolve<HostClient>())
  {
    AddTool("Initialize-Session", &SessionToolSource::InitializeSession);
    AddTool("Check-Session", &SessionToolSource::CheckSession);
    AddTool("Shutdown-Session", &SessionToolSource::ShutdownSession);
    AddTool("Run-Sequence", &SessionToolSource::RunSequence);
    AddTool("Set-Display", &SessionToolSource::Forward<SetDisplayRequest>);
    AddTool("Get-Display", &SessionToolSource::Forward<GetDisplayRequest>);
    AddTool("Configure-Capture", &SessionToolSource::Forward<ConfigureCaptureRequest>);
  }

  RpcResult<SessionResponse> SessionToolSource::InitializeSession(const InitializeSessionRequest& arguments, ToolContext& context)
  {
    if (auto error = _host->Start(context.Cancellation)) return *error;

    return _host->Request(arguments, HostTimeout(arguments.Timeout, InitializeSessionRequest::DefaultTimeout), context.Cancellation);
  }

  RpcResult<SessionResponse> SessionToolSource::CheckSession(const CheckSessionRequest& arguments, ToolContext& context)
  {
    //Without the host there is no session, which is an answer rather than an error
    if (!_host->IsRunning()) return SessionResponse{};

    return Forward(arguments, context);
  }

  RpcResult<EmptyResponse> SessionToolSource::ShutdownSession(const ShutdownSessionRequest& arguments, ToolContext& context)
  {
    return _host->Request(arguments, HostTimeout(arguments.Timeout, ShutdownSessionRequest::DefaultTimeout), context.Cancellation);
  }

  RpcResult<RunSequenceResponse> SessionToolSource::RunSequence(const RunSequenceArguments& arguments, ToolContext& context)
  {
    RunSequenceRequest request;
    static_cast<RunSequenceArguments&>(request) = arguments;

    //Forward the progress of this sequence; the host sends it before the response, on the connection's thread, so it
    //never arrives after this returns
    event_subscription progressSubscription;
    if (context.ReportProgress)
    {
      request.ProgressToken = make_value<json_number>(double(_nextProgressToken++));
      progressSubscription = _host->SequenceProgress([&context, token = request.ProgressToken->to_string()](HostClient*, const SequenceProgressNotification& progress) {
        if (progress.ProgressToken && progress.ProgressToken->to_string() == token) context.ReportProgress(progress.CompletedSteps, progress.TotalSteps, progress.Message);
        });
    }

    auto result = _host->Request(request, HostTimeout(arguments.Timeout, RunSequenceArguments::DefaultTimeout), context.Cancellation);
    if (auto response = get_if<RunSequenceResponse>(&result)) MoveImagesToContent(*response, context);
    return result;
  }

  template<typename TRequest>
  RpcResult<typename TRequest::ResponseType> SessionToolSource::Forward(const TRequest& arguments, ToolContext& context)
  {
    return _host->Request(arguments, _hostTimeout, context.Cancellation);
  }

  void SessionToolSource::MoveImagesToContent(RunSequenceResponse& response, ToolContext& context)
  {
    for (auto& result : response.Results)
    {
      auto capture = dynamic_cast<CaptureResult*>(result.get());
      if (!capture || capture->Image.Data.empty()) continue;

      //A caption ties the image to its capture in the structured result, which keeps everything but the data
      auto& image = capture->Image;
      auto caption = make_value<TextContent>();
      caption->Text = format("Capture {}: {}x{} pixels, scale {}.", capture->Tag, image.Width, image.Height, image.Scale);
      context.Content.push_back(move(caption));

      auto content = make_value<ImageContent>();
      content->Data = exchange(image.Data, {});
      content->MimeType = image.Format == ImageFormat::Jpeg ? "image/jpeg" : "image/png";
      context.Content.push_back(move(content));
    }
  }
}
