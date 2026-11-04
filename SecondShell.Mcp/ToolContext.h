#pragma once
#include "CancellationToken.h"
#include "McpContent.h"

namespace SecondShell::Mcp
{
  //What a tool call has besides its arguments.
  struct ToolContext
  {
    const Rpc::CancellationToken& Cancellation;

    //Reports progress to the client, which must increase with each report; empty if the client did not ask for
    //progress. May be called from any thread.
    std::function<void(double progress, double total, std::string_view message)> ReportProgress;

    //Added to the response after the result, e.g. the images of captures.
    std::vector<Axodox::Infrastructure::value_ptr<ContentBlock>> Content;
  };
}
