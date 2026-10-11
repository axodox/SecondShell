#pragma once
#include "IMessageTransport.h"

namespace SecondShell::Mcp
{
  //MCP's stdio transport: one JSON-RPC message per line on stdin and stdout. Takes over stdout, so that nothing else
  //written there (by a library or a log) reaches the client: everything else goes to stderr.
  class StdioTransport : public Rpc::IMessageTransport
  {
  public:
    StdioTransport();

    virtual void Send(std::string_view message) override;
    virtual bool IsOpen() const override;

    //Reads messages on the calling thread until the client closes stdin.
    void Run();

  private:
    winrt::file_handle _output;
    std::mutex _outputMutex;
    std::atomic_bool _isOpen = true;
  };
}
