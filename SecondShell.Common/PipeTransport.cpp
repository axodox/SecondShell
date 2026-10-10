#include "pch.h"
#include "PipeTransport.h"

using namespace Axodox::Networking;
using namespace std;

namespace SecondShell::Rpc
{
  PipeTransport::PipeTransport(named_pipe_messaging_channel* channel) :
    _channel(channel),
    _messageReceivedSubscription(_channel->message_received({ this, &PipeTransport::OnChannelMessageReceived })),
    _disconnectedSubscription(_channel->disconnected({ this, &PipeTransport::OnChannelDisconnected }))
  { }

  void PipeTransport::Send(string_view message)
  {
    if (!_channel->is_connected()) throw runtime_error("The pipe is closed.");

    _channel->send_message(vector<uint8_t>(message.begin(), message.end()));
  }

  bool PipeTransport::IsOpen() const
  {
    return _channel->is_connected();
  }

  uint32_t PipeTransport::PeerSessionId() const
  {
    return _channel->peer_session_id();
  }

  uint32_t PipeTransport::PeerProcessId() const
  {
    return _channel->peer_process_id();
  }

  void PipeTransport::OnChannelMessageReceived(messaging_channel* /*channel*/, span<const uint8_t> message)
  {
    OnMessageReceived(string_view(reinterpret_cast<const char*>(message.data()), message.size()));
  }

  void PipeTransport::OnChannelDisconnected(messaging_channel* /*channel*/)
  {
    OnClosed();
  }
}
