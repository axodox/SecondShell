#pragma once
#include "IMessageTransport.h"

namespace SecondShell::Rpc
{
  //A transport over one connection of a named pipe messaging server or client, one message per pipe message.
  //Create it in the connection's connected event, so no message arrives before it subscribes. The channel belongs to
  //the server or client, which may destroy it after it disconnects: from then on, the transport no longer touches it.
  class SECONDSHELL_COMMON_API PipeTransport : public IMessageTransport
  {
  public:
    explicit PipeTransport(Axodox::Networking::named_pipe_messaging_channel* channel);

    virtual void Send(std::string_view message) override;
    virtual bool IsOpen() const override;

    uint32_t PeerSessionId() const;
    uint32_t PeerProcessId() const;

  private:
    mutable std::mutex _mutex;
    Axodox::Networking::named_pipe_messaging_channel* _channel;
    Axodox::Networking::named_pipe_peer_description _remotePeer;
    Axodox::Infrastructure::event_subscription _messageReceivedSubscription;
    Axodox::Infrastructure::event_subscription _disconnectedSubscription;

    void OnChannelMessageReceived(Axodox::Networking::messaging_channel* channel, std::span<const uint8_t> message);
    void OnChannelDisconnected(Axodox::Networking::messaging_channel* channel);
  };
}
