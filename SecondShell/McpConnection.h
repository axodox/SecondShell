#pragma once
#include "CaptureStore.h"
#include "HostProtocol.h"
#include "JsonRpcPeer.h"
#include "PipeTransport.h"
#include "SequenceRunner.h"
#include "SessionManager.h"

namespace SecondShell::Agent
{
  //One MCP server connected to the host: answers its requests (one per MCP tool) with the services.
  class McpConnection
  {
  public:
    McpConnection(Axodox::Infrastructure::dependency_container* container, Axodox::Networking::named_pipe_messaging_channel* channel);

  private:
    std::shared_ptr<Session::SessionManager> _sessions;
    std::shared_ptr<SequenceRunner> _sequences;
    std::shared_ptr<CaptureStore> _captures;
    Rpc::JsonRpcPeer _peer;

    Rpc::RpcResult<Protocol::SessionResponse> InitializeSession(const Protocol::InitializeSessionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::SessionResponse> CheckSession(const Protocol::CheckSessionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::EmptyResponse> ShutdownSession(const Protocol::ShutdownSessionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::RunSequenceResponse> RunSequence(const Protocol::RunSequenceRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::DisplayResponse> SetDisplay(const Protocol::SetDisplayRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::DisplayResponse> GetDisplay(const Protocol::GetDisplayRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::CaptureSettingsResponse> ConfigureCapture(const Protocol::ConfigureCaptureRequest& request, const Rpc::CancellationToken& cancellation);
  };
}
