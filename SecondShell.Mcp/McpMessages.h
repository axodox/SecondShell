#pragma once
#include "EmptyResponse.h"
#include "McpContent.h"

//The MCP messages of a tools-only server (https://modelcontextprotocol.io/specification/2025-11-25). Each request's
//descriptor name is its JSON-RPC method.
namespace SecondShell::Mcp
{
#pragma region Initialization
  //The newest revision first: it is answered when the client asks for one this server does not know.
  inline constexpr std::array<std::string_view, 4> SupportedProtocolVersions = { "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05" };

  struct ImplementationInfo
  {
    static Axodox::Json::json_object_descriptor<ImplementationInfo> json_description;

    std::string Name;
    std::string Version;
  };

  struct ToolsCapability
  {
    static Axodox::Json::json_object_descriptor<ToolsCapability> json_description;

    bool ListChanged = false;
  };

  struct ServerCapabilities
  {
    static Axodox::Json::json_object_descriptor<ServerCapabilities> json_description;

    ToolsCapability Tools;
  };

  struct InitializeResponse
  {
    static Axodox::Json::json_object_descriptor<InitializeResponse> json_description;

    std::string ProtocolVersion;
    ServerCapabilities Capabilities;
    ImplementationInfo ServerInfo;
    std::string Instructions;
  };

  struct InitializeRequest
  {
    static Axodox::Json::json_object_descriptor<InitializeRequest> json_description;
    using ResponseType = InitializeResponse;

    std::string ProtocolVersion;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Capabilities;
    std::optional<ImplementationInfo> ClientInfo;
  };

  struct PingRequest
  {
    static Axodox::Json::json_object_descriptor<PingRequest> json_description;
    using ResponseType = Protocol::EmptyResponse;
  };
#pragma endregion

#pragma region Tools
  struct ToolDescription
  {
    static Axodox::Json::json_object_descriptor<ToolDescription> json_description;

    std::string Name;
    std::string Description;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> InputSchema;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> OutputSchema;
  };

  struct ListToolsResponse
  {
    static Axodox::Json::json_object_descriptor<ListToolsResponse> json_description;

    std::vector<ToolDescription> Tools;
  };

  //All tools fit on one page, so the cursor is ignored.
  struct ListToolsRequest
  {
    static Axodox::Json::json_object_descriptor<ListToolsRequest> json_description;
    using ResponseType = ListToolsResponse;
  };

  struct CallToolResponse
  {
    static Axodox::Json::json_object_descriptor<CallToolResponse> json_description;

    std::vector<Axodox::Infrastructure::value_ptr<ContentBlock>> Content;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> StructuredContent;
    bool IsError = false;

    //A tool error, which the AI sees and can react to, unlike a JSON-RPC error.
    static CallToolResponse Error(std::string message);
  };

  struct RequestMeta
  {
    static Axodox::Json::json_object_descriptor<RequestMeta> json_description;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ProgressToken;
  };

  struct CallToolRequest
  {
    static Axodox::Json::json_object_descriptor<CallToolRequest> json_description;
    using ResponseType = CallToolResponse;

    std::string Name;
    Axodox::Json::json_object Arguments;
    std::optional<RequestMeta> Meta;
  };
#pragma endregion

#pragma region Notifications
  struct ProgressNotification
  {
    static Axodox::Json::json_object_descriptor<ProgressNotification> json_description;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ProgressToken;
    double Progress = 0;
    std::optional<double> Total;
    std::optional<std::string> Message;
  };
#pragma endregion
}
