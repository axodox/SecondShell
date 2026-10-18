#include "pch.h"
#include "McpConnection.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Agent
{
  McpConnection::McpConnection(dependency_container* container, named_pipe_messaging_channel* channel) :
    _sessions(container->resolve<SessionManager>()),
    _sequences(container->resolve<SequenceRunner>()),
    _captures(container->resolve<CaptureStore>()),
    _peer(make_shared<PipeTransport>(channel))
  {
    _peer.AddHandler(this, &McpConnection::InitializeSession);
    _peer.AddHandler(this, &McpConnection::CheckSession);
    _peer.AddHandler(this, &McpConnection::ShutdownSession);
    _peer.AddHandler(this, &McpConnection::RunSequence);
    _peer.AddHandler(this, &McpConnection::SetDisplay);
    _peer.AddHandler(this, &McpConnection::GetDisplay);
    _peer.AddHandler(this, &McpConnection::ConfigureCapture);
    _peer.Start();
  }

  RpcResult<SessionResponse> McpConnection::InitializeSession(const InitializeSessionRequest& request, const CancellationToken& cancellation)
  {
    if (request.Capture) _captures->Configure(*request.Capture);
    if (auto error = _sessions->Start(request, cancellation)) return *error;

    return SessionResponse{ _sessions->Info() };
  }

  RpcResult<SessionResponse> McpConnection::CheckSession(const CheckSessionRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    return SessionResponse{ _sessions->Info() };
  }

  RpcResult<EmptyResponse> McpConnection::ShutdownSession(const ShutdownSessionRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    _sessions->Shutdown();
    return EmptyResponse{};
  }

  RpcResult<RunSequenceResponse> McpConnection::RunSequence(const RunSequenceRequest& request, const CancellationToken& cancellation)
  {
    ProgressCallback progress;
    if (request.ProgressToken)
    {
      progress = [&](uint32_t completed, uint32_t total, string_view message) {
        _peer.Notify(SequenceProgressNotification{ request.ProgressToken, completed, total, string(message) });
        };
    }

    return _sequences->Run(request, progress, cancellation);
  }

  RpcResult<DisplayResponse> McpConnection::SetDisplay(const SetDisplayRequest& request, const CancellationToken& cancellation)
  {
    if (auto error = _sessions->SetDisplay(request.Display, cancellation)) return *error;
    return DisplayResponse{ *_sessions->Info().Display };
  }

  RpcResult<DisplayResponse> McpConnection::GetDisplay(const GetDisplayRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    auto display = _sessions->Info().Display;
    if (!display) return ProtocolErrors::Make(ErrorCode::SessionNotActive, "No session is running; start one with Initialize-Session.");

    return DisplayResponse{ *display };
  }

  RpcResult<CaptureSettingsResponse> McpConnection::ConfigureCapture(const ConfigureCaptureRequest& request, const CancellationToken& /*cancellation*/)
  {
    _captures->Configure(request.Capture);
    return CaptureSettingsResponse{ _captures->Settings() };
  }
}
