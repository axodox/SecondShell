#include "pch.h"
#include "IMessageTransport.h"

using namespace std;

namespace SecondShell::Rpc
{
  IMessageTransport::IMessageTransport() :
    MessageReceived(_events),
    Closed(_events)
  { }

  void IMessageTransport::OnMessageReceived(string_view message)
  {
    _events.raise(MessageReceived, this, message);
  }

  void IMessageTransport::OnClosed()
  {
    _events.raise(Closed, this);
  }
}
