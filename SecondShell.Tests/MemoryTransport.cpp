#include "pch.h"
#include "MemoryTransport.h"

using namespace Axodox::Threading;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  pair<shared_ptr<MemoryTransport>, shared_ptr<MemoryTransport>> MemoryTransport::CreatePair()
  {
    auto first = make_shared<MemoryTransport>();
    auto second = make_shared<MemoryTransport>();
    first->_other = second;
    second->_other = first;
    return { first, second };
  }

  MemoryTransport::MemoryTransport() :
    _reader([this] { ReadMessages(); }, "* memory transport reader")
  { }

  MemoryTransport::~MemoryTransport()
  {
    _inbox.complete();
    _reader.reset();
  }

  void MemoryTransport::Send(string_view message)
  {
    auto other = _other.lock();
    if (!_isOpen || !other) throw runtime_error("The transport is closed.");

    other->_inbox.add(string(message));
  }

  bool MemoryTransport::IsOpen() const
  {
    return _isOpen;
  }

  void MemoryTransport::Close()
  {
    CloseThisEnd();
    if (auto other = _other.lock()) other->CloseThisEnd();
  }

  void MemoryTransport::ReadMessages()
  {
    while (true)
    {
      string message;
      if (_inbox.try_get(message, 100ms))
      {
        OnMessageReceived(message);
      }
      else if (_inbox.is_complete())
      {
        break;
      }
    }
  }

  void MemoryTransport::CloseThisEnd()
  {
    if (_isOpen.exchange(false)) OnClosed();
  }
}
