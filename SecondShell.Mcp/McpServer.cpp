#include "pch.h"
#include "McpServer.h"
#include "ProductInfo.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Environment;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Mcp
{
  const char* const _instructions =
    "SecondShell gives you a second Windows desktop: a separate session of the same user on this machine, where you can "
    "use applications without disturbing the user. Start it with Initialize-Session, then work in it with Run-Sequence "
    "(mouse, keyboard, touch, screen captures, windows, processes, clipboard). Coordinates are session pixels; captures "
    "report the scale of their image. Applications in the session work on the user's real files and settings.";

  McpServer::McpServer(dependency_container* container) :
    _toolSources(container->resolve<registry<ToolSource>>()),
    _peer(container->resolve<IMessageTransport>())
  {
    _peer.AddHandler(this, &McpServer::Initialize);
    _peer.AddHandler(this, &McpServer::Ping);
    _peer.AddHandler(this, &McpServer::ListTools);
    _peer.AddHandler(this, &McpServer::CallTool);
  }

  void McpServer::Start()
  {
    _peer.Start();
  }

  RpcResult<InitializeResponse> McpServer::Initialize(const InitializeRequest& request, const CancellationToken& /*cancellation*/)
  {
    if (request.ClientInfo) _logger.log(log_severity::information, "Client: {} {}, MCP {}.", request.ClientInfo->Name, request.ClientInfo->Version, request.ProtocolVersion);

    //Answer in the client's revision if it is supported, otherwise in the newest one
    auto version = ranges::find(SupportedProtocolVersions, string_view(request.ProtocolVersion));
    return InitializeResponse{
      .ProtocolVersion = string(version != SupportedProtocolVersions.end() ? *version : SupportedProtocolVersions.front()),
      .ServerInfo = { ProductInfo::Name(), ProductInfo::Version() },
      .Instructions = _instructions
    };
  }

  RpcResult<EmptyResponse> McpServer::Ping(const PingRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    return EmptyResponse{};
  }

  RpcResult<ListToolsResponse> McpServer::ListTools(const ListToolsRequest& /*request*/, const CancellationToken& /*cancellation*/)
  {
    ListToolsResponse response;
    for (auto& source : **_toolSources)
    {
      for (auto& tool : source->Tools())
      {
        response.Tools.push_back({ tool.Name, tool.Description, tool.InputSchema, tool.OutputSchema });
      }
    }
    return response;
  }

  RpcResult<CallToolResponse> McpServer::CallTool(const CallToolRequest& request, const CancellationToken& cancellation)
  {
    auto tool = FindTool(request.Name);
    if (!tool) return JsonRpcError{ JsonRpcErrorCode::InvalidParams, format("Unknown tool {}.", request.Name) };

    ToolContext context{ .Cancellation = cancellation, .ReportProgress = ProgressReporter(request) };
    try
    {
      return tool->Call(request.Arguments, context);
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::error, "Tool {} failed: {}", request.Name, exception.what());
      return CallToolResponse::Error(format("The tool failed: {}", exception.what()));
    }
  }

  const ToolDefinition* McpServer::FindTool(string_view name) const
  {
    //The sources are registered for the lifetime of the process, so their tools outlive the snapshot
    for (auto& source : **_toolSources)
    {
      for (auto& tool : source->Tools())
      {
        if (tool.Name == name) return &tool;
      }
    }
    return nullptr;
  }

  function<void(uint32_t, uint32_t, string_view)> McpServer::ProgressReporter(const CallToolRequest& request)
  {
    if (!request.Meta || !request.Meta->ProgressToken) return {};

    return [this, token = request.Meta->ProgressToken](uint32_t completed, uint32_t total, string_view message) {
      _peer.Notify(ProgressNotification{
        .ProgressToken = token,
        .Progress = double(completed),
        .Total = double(total),
        .Message = message.empty() ? nullopt : optional<string>(message)
        });
      };
  }
}
