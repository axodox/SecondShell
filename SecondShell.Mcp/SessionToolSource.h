#pragma once
#include "HostClient.h"
#include "ToolSource.h"

namespace SecondShell::Mcp
{
  //The SecondShell tools (Design.md, MCP tools); each forwards to the host. Their arguments are the host's request
  //types, except Run-Sequence's, which leave out the progress token.
  class SessionToolSource : public ToolSource
  {
  public:
    explicit SessionToolSource(Axodox::Infrastructure::dependency_container* container);

  private:
    std::shared_ptr<HostClient> _host;
    std::atomic_uint64_t _nextProgressToken = 1;

    Rpc::RpcResult<Protocol::SessionResponse> InitializeSession(const Protocol::InitializeSessionRequest& arguments, ToolContext& context);
    Rpc::RpcResult<Protocol::SessionResponse> CheckSession(const Protocol::CheckSessionRequest& arguments, ToolContext& context);
    Rpc::RpcResult<Protocol::EmptyResponse> ShutdownSession(const Protocol::ShutdownSessionRequest& arguments, ToolContext& context);
    Rpc::RpcResult<Protocol::RunSequenceResponse> RunSequence(const Protocol::RunSequenceArguments& arguments, ToolContext& context);

    //The tools which only forward their request.
    template<typename TRequest>
    Rpc::RpcResult<typename TRequest::ResponseType> Forward(const TRequest& arguments, ToolContext& context);

    //Captures returned inline become image content, which the AI can see.
    static void MoveImagesToContent(Protocol::RunSequenceResponse& response, ToolContext& context);
    static void AddImage(Actions::CapturedImage& image, std::string_view name, ToolContext& context);
  };
}
