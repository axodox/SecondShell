#include "pch.h"
#include "OverlayElement.h"

using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  float OverlayElement::Progress(steady_clock::time_point start, steady_clock::duration duration, steady_clock::time_point now)
  {
    if (duration <= 0s) return 1.f;
    return clamp(float(duration_cast<microseconds>(now - start).count()) / float(duration_cast<microseconds>(duration).count()), 0.f, 1.f);
  }

  float OverlayElement::EaseOut(float progress)
  {
    auto remaining = 1.f - progress;
    return 1.f - remaining * remaining * remaining;
  }
}
