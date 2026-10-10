#include "pch.h"
#include "CancellationToken.h"

using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Rpc
{
  CancellationToken::CancellationToken() :
    Cancelled(_events)
  { }

  void CancellationToken::Cancel()
  {
    if (_isCancelled.exchange(true)) return;

    _cancelledEvent.set();
    _events.raise(Cancelled, this);
  }

  bool CancellationToken::IsCancelled() const
  {
    return _isCancelled;
  }

  bool CancellationToken::WaitFor(chrono::milliseconds timeout) const
  {
    //A zero timeout would make the event wait forever
    if (_isCancelled || timeout <= 0ms) return _isCancelled;

    return _cancelledEvent.wait(timeout);
  }
}
