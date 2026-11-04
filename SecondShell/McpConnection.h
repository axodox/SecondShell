#pragma once
#include "AgentControl.h"
#include "CaptureStore.h"
#include "HostProtocol.h"
#include "IDriverLink.h"
#include "JsonRpcPeer.h"
#include "LockScreenBridge.h"
#include "PipeTransport.h"
#include "SequenceRunner.h"
#include "SessionEventLog.h"
#include "SessionManager.h"

namespace SecondShell::Agent
{
  //One MCP server connected to the host: answers its requests (one per MCP tool) with the services, and tells the
  //agent with each answer what changed since its previous call (Design.md, Telling the agent what changed).
  class McpConnection
  {
    inline static const Axodox::Infrastructure::logger _logger{ "McpConnection" };

  public:
    McpConnection(Axodox::Infrastructure::dependency_container* container, Axodox::Networking::named_pipe_messaging_channel* channel);

  private:
    std::shared_ptr<Session::SessionManager> _sessions;
    std::shared_ptr<SequenceRunner> _sequences;
    std::shared_ptr<CaptureStore> _captures;
    std::shared_ptr<Session::LockScreenBridge> _lockScreen;
    std::shared_ptr<AgentControl> _agentControl;
    std::shared_ptr<SessionEventLog> _eventLog;
    std::shared_ptr<Session::IDriverLink> _driver;

    //What this agent has been told; requests may run in parallel
    std::mutex _noticesMutex;
    uint64_t _eventPosition = 0;
    NewWindowFilter _newWindows;

    Rpc::JsonRpcPeer _peer;

    //The session, with whether the user is at the PC, whether input reaches the session, and who controls it.
    Protocol::SessionInfo Info() const;

    //Whether the agent may send input now: it has control, and input reaches the session.
    bool CanSendInput() const;

    //Why input is unavailable, and whether retrying soon helps.
    std::string InputUnavailableMessage() const;

    //The events and new windows since the previous call.
    void AddNotices(std::vector<Protocol::SessionEvent>& events, std::vector<Actions::WindowInfo>& newWindows);
    std::optional<std::vector<Actions::WindowInfo>> Windows();  //of the session, dialogs included; none if listing failed

    Rpc::RpcResult<Protocol::SessionResponse> InitializeSession(const Protocol::InitializeSessionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::SessionResponse> CheckSession(const Protocol::CheckSessionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::EmptyResponse> ShutdownSession(const Protocol::ShutdownSessionRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::RunSequenceResponse> RunSequence(const Protocol::RunSequenceRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::DisplayResponse> SetDisplay(const Protocol::SetDisplayRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::DisplayResponse> GetDisplay(const Protocol::GetDisplayRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::CaptureSettingsResponse> ConfigureCapture(const Protocol::ConfigureCaptureRequest& request, const Rpc::CancellationToken& cancellation);
  };
}
