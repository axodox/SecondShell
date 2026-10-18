#include "pch.h"
#include "FrameRateMeter.h"

using namespace std;
using namespace std::chrono;

namespace SecondShell::Graphics
{
  FrameRateMeter::FrameRateMeter(steady_clock::duration period) :
    _period(period)
  { }

  optional<float> FrameRateMeter::Tick()
  {
    auto now = steady_clock::now();

    //A pause longer than the period starts a new one, so the rate is of frames which kept coming
    if (now - _start > 2 * _period)
    {
      _start = now;
      _count = 0;
      return nullopt;
    }

    _count++;
    auto elapsed = now - _start;
    if (elapsed < _period) return nullopt;

    auto rate = _count / duration<float>(elapsed).count();
    _start = now;
    _count = 0;
    return rate;
  }
}
