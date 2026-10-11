#pragma once
#include "CancellationToken.h"

namespace SecondShell::Input
{
  //Spreads a motion over a duration: calls step with the progress (0..1], linearly, about every 10 ms, and always ends
  //with 1 unless cancelled.
  struct MotionAnimator
  {
    //False if it was cancelled.
    static bool Animate(std::chrono::milliseconds duration, const std::function<void(float progress)>& step, const Rpc::CancellationToken& cancellation);
  };
}
