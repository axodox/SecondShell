#include "pch.h"
#include "MotionAnimator.h"

using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Input
{
  const milliseconds _stepInterval{ 10 };

  bool MotionAnimator::Animate(milliseconds duration, const function<void(float progress)>& step, const CancellationToken& cancellation)
  {
    auto start = steady_clock::now();
    while (true)
    {
      auto elapsed = steady_clock::now() - start;
      auto progress = duration.count() > 0 ? min(float(duration_cast<microseconds>(elapsed).count()) / float(duration_cast<microseconds>(duration).count()), 1.f) : 1.f;
      step(progress);

      if (progress >= 1.f) return true;
      if (cancellation.WaitFor(_stepInterval)) return false;
    }
  }
}
