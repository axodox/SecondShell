#pragma once
#include "CommonApi.h"

namespace SecondShell::Rpc
{
  //Moves message text (one JSON-RPC message per message) between two processes, and nothing else.
  class SECONDSHELL_COMMON_API IMessageTransport
  {
    Axodox::Infrastructure::event_owner _events;

  public:
    virtual ~IMessageTransport() = default;

    //Thread-safe; throws if the transport is closed.
    virtual void Send(std::string_view message) = 0;
    virtual bool IsOpen() const = 0;

    //Raised on the transport's reading thread, in the order the messages arrived.
    Axodox::Infrastructure::event_publisher<IMessageTransport*, std::string_view> MessageReceived;
    Axodox::Infrastructure::event_publisher<IMessageTransport*> Closed;

  protected:
    IMessageTransport();

    void OnMessageReceived(std::string_view message);
    void OnClosed();
  };
}
