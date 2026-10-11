#pragma once
#include "HostLauncher.h"
#include "HostProtocol.h"
#include "JsonRpcPeer.h"
#include "PipeTransport.h"
#include "ProtocolErrors.h"

namespace SecondShell::Mcp
{
  //The JSON-RPC connection to the host. Connects when it is needed: on first use, and again after the host restarted.
  class HostClient
  {
    inline static const Axodox::Infrastructure::logger _logger{ "HostClient" };
    Axodox::Infrastructure::event_owner _events;

    struct Connection;

  public:
    explicit HostClient(Axodox::Infrastructure::dependency_container* container);
    HostClient(Axodox::Infrastructure::dependency_container* container, std::string pipeName);
    ~HostClient();

    bool IsRunning();

    //Starts the host unless it is running, and connects to it. Returns an error if it could not.
    std::optional<Rpc::JsonRpcError> Start(const Rpc::CancellationToken& cancellation);

    //Fails with SessionNotActive if the host is not running.
    template<typename TRequest, typename TResponse = typename TRequest::ResponseType>
    Rpc::RpcResult<TResponse> Request(const TRequest& request, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation)
    {
      auto connection = Connect();
      if (!connection) return Protocol::ProtocolErrors::Make(Actions::ErrorCode::SessionNotActive, "SecondShell is not running; start a session with Initialize-Session.");

      return Peer(*connection).template Request<TRequest, TResponse>(request, timeout, cancellation);
    }

    //Raised on the connection's thread, before the response of the sequence.
    Axodox::Infrastructure::event_publisher<HostClient*, const Protocol::SequenceProgressNotification&> SequenceProgress;

  private:
    std::shared_ptr<HostLauncher> _launcher;
    std::string _pipeName;

    std::mutex _mutex;
    std::shared_ptr<Connection> _connection;

    std::shared_ptr<Connection> Connect();
    static Rpc::JsonRpcPeer& Peer(Connection& connection);

    void OnSequenceProgress(const Protocol::SequenceProgressNotification& notification);
  };
}
