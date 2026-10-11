#include "pch.h"
#include "McpMessages.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace std;

namespace SecondShell::Mcp
{
#pragma region Initialization
  json_object_descriptor<ImplementationInfo> ImplementationInfo::json_description = describe_json_object<ImplementationInfo>({
    { &ImplementationInfo::Name, "name", { .is_required = true } },
    { &ImplementationInfo::Version, "version", { .is_required = true } }
    });

  json_object_descriptor<ToolsCapability> ToolsCapability::json_description = describe_json_object<ToolsCapability>({
    { &ToolsCapability::ListChanged, "listChanged" }
    });

  json_object_descriptor<ServerCapabilities> ServerCapabilities::json_description = describe_json_object<ServerCapabilities>({
    { &ServerCapabilities::Tools, "tools" }
    });

  json_object_descriptor<InitializeResponse> InitializeResponse::json_description = describe_json_object<InitializeResponse>({
    { &InitializeResponse::ProtocolVersion, "protocolVersion" },
    { &InitializeResponse::Capabilities, "capabilities" },
    { &InitializeResponse::ServerInfo, "serverInfo" },
    { &InitializeResponse::Instructions, "instructions", { .is_required = false } }
    });

  json_object_descriptor<InitializeRequest> InitializeRequest::json_description = describe_json_object<InitializeRequest>(
    { .name = "initialize" },
    {
      { &InitializeRequest::ProtocolVersion, "protocolVersion", { .is_required = true } },
      { &InitializeRequest::Capabilities, "capabilities", { .is_required = false } },
      { &InitializeRequest::ClientInfo, "clientInfo", { .is_required = false } }
    });

  json_object_descriptor<PingRequest> PingRequest::json_description = describe_json_object<PingRequest>({ .name = "ping" }, {});
#pragma endregion

#pragma region Tools
  json_object_descriptor<ToolDescription> ToolDescription::json_description = describe_json_object<ToolDescription>({
    { &ToolDescription::Name, "name" },
    { &ToolDescription::Description, "description" },
    { &ToolDescription::InputSchema, "inputSchema" },
    { &ToolDescription::OutputSchema, "outputSchema", { .is_required = false } }
    });

  json_object_descriptor<ListToolsResponse> ListToolsResponse::json_description = describe_json_object<ListToolsResponse>({
    { &ListToolsResponse::Tools, "tools" }
    });

  json_object_descriptor<ListToolsRequest> ListToolsRequest::json_description = describe_json_object<ListToolsRequest>({ .name = "tools/list" }, {});

  json_object_descriptor<CallToolResponse> CallToolResponse::json_description = describe_json_object<CallToolResponse>({
    { &CallToolResponse::Content, "content" },
    { &CallToolResponse::StructuredContent, "structuredContent", { .is_required = false } },
    { &CallToolResponse::IsError, "isError", { .is_required = false } }
    });

  CallToolResponse CallToolResponse::Error(string message)
  {
    auto text = make_value<TextContent>();
    text->Text = move(message);

    CallToolResponse result;
    result.Content.push_back(move(text));
    result.IsError = true;
    return result;
  }

  json_object_descriptor<RequestMeta> RequestMeta::json_description = describe_json_object<RequestMeta>({
    { &RequestMeta::ProgressToken, "progressToken", { .is_required = false } }
    });

  json_object_descriptor<CallToolRequest> CallToolRequest::json_description = describe_json_object<CallToolRequest>(
    { .name = "tools/call" },
    {
      { &CallToolRequest::Name, "name", { .is_required = true } },
      { &CallToolRequest::Arguments, "arguments", { .is_required = false } },
      { &CallToolRequest::Meta, "_meta", { .is_required = false } }
    });
#pragma endregion

#pragma region Notifications
  json_object_descriptor<ProgressNotification> ProgressNotification::json_description = describe_json_object<ProgressNotification>(
    { .name = "notifications/progress" },
    {
      { &ProgressNotification::ProgressToken, "progressToken", { .is_required = true } },
      { &ProgressNotification::Progress, "progress" },
      { &ProgressNotification::Total, "total", { .is_required = false } },
      { &ProgressNotification::Message, "message", { .is_required = false } }
    });
#pragma endregion
}
