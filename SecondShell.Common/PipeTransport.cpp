#include "pch.h"
#include "PipeTransport.h"

using namespace Axodox::Networking;
using namespace std;

namespace SecondShell::Rpc
{
  PipeTransport::PipeTransport(named_pipe_messaging_channel* channel) :
    _channel(channel),
    _peerSessionId(channel->peer_session_id()),
    _peerProcessId(channel->peer_process_id()),
    _messageReceivedSubscription(_channel->message_received({ this, &PipeTransport::OnChannelMessageReceived })),
    _disconnectedSubscription(_channel->disconnected({ this, &PipeTransport::OnChannelDisconnected }))
  { }

  void PipeTransport::Send(string_view message)
  {
    lock_guard lock{ _mutex };
    if (!_channel || !_channel->is_connected()) throw runtime_error("The pipe is closed.");

    _channel->send_message(vector<uint8_t>(message.begin(), message.end()));
  }

  bool PipeTransport::IsOpen() const
  {
    lock_guard lock{ _mutex };
    return _channel && _channel->is_connected();
  }

  uint32_t PipeTransport::PeerSessionId() const
  {
    return _peerSessionId;
  }

  uint32_t PipeTransport::PeerProcessId() const
  {
    return _peerProcessId;
  }

  void PipeTransport::OnChannelMessageReceived(messaging_channel* /*channel*/, span<const uint8_t> message)
  {
    OnMessageReceived(string_view(reinterpret_cast<const char*>(message.data()), message.size()));
  }

  void PipeTransport::OnChannelDisconnected(messaging_channel* /*channel*/)
  {
    //Waits for a send in progress; the channel may be destroyed once this returns
    {
      lock_guard lock{ _mutex };
      _channel = nullptr;
    }

    OnClosed();
  }
}
