#pragma once
#include "CommonApi.h"

namespace SecondShell::Rpc
{
  //Cancellation of a running operation. Cancel may be called from any thread, any number of times.
  class SECONDSHELL_COMMON_API CancellationToken
  {
    Axodox::Infrastructure::event_owner _events;

  public:
    CancellationToken();

    CancellationToken(const CancellationToken&) = delete;
    CancellationToken& operator=(const CancellationToken&) = delete;

    void Cancel();
    bool IsCancelled() const;

    //Waits until the token is cancelled or the timeout expires; returns true if it was cancelled.
    bool WaitFor(std::chrono::milliseconds timeout) const;

    //Raised once, on the thread which cancels. Subscribers should check IsCancelled after subscribing.
    //Mutable, so that code which only observes a token (const CancellationToken&) can subscribe.
    mutable Axodox::Infrastructure::event_publisher<CancellationToken*> Cancelled;

  private:
    mutable Axodox::Threading::manual_reset_event _cancelledEvent;
    std::atomic_bool _isCancelled = false;
  };
}
