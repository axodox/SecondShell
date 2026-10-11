#pragma once
#include "JsonRpcPeer.h"
#include "McpMessages.h"
#include "ToolSource.h"

namespace SecondShell::Mcp
{
  //The MCP methods on the JSON-RPC peer of the client's transport: initialize (version negotiation, capabilities),
  //ping, tools/list and tools/call, which go to the tools of registry<ToolSource>. Cancellation and progress use MCP's
  //notifications.
  class McpServer
  {
    inline static const Axodox::Infrastructure::logger _logger{ "McpServer" };

  public:
    explicit McpServer(Axodox::Infrastructure::dependency_container* container);

    //Starts answering, after the tool sources are registered.
    void Start();

  private:
    std::shared_ptr<Axodox::Infrastructure::registry<ToolSource>> _toolSources;
    Rpc::JsonRpcPeer _peer;

    Rpc::RpcResult<InitializeResponse> Initialize(const InitializeRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<Protocol::EmptyResponse> Ping(const PingRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<ListToolsResponse> ListTools(const ListToolsRequest& request, const Rpc::CancellationToken& cancellation);
    Rpc::RpcResult<CallToolResponse> CallTool(const CallToolRequest& request, const Rpc::CancellationToken& cancellation);

    const ToolDefinition* FindTool(std::string_view name) const;
    std::function<void(uint32_t, uint32_t, std::string_view)> ProgressReporter(const CallToolRequest& request);
  };
}
