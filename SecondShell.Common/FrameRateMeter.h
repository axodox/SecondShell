#pragma once
#include "CommonApi.h"

namespace SecondShell::Graphics
{
  //Counts frames, and gives their rate once per period.
  class SECONDSHELL_COMMON_API FrameRateMeter
  {
  public:
    explicit FrameRateMeter(std::chrono::steady_clock::duration period);

    //The frames per second of the period this frame ends, else nothing.
    std::optional<float> Tick();

  private:
    std::chrono::steady_clock::duration _period;
    std::chrono::steady_clock::time_point _start{};
    uint32_t _count = 0;
  };
}
