#pragma once
#include "Point.h"

namespace SecondShell::Input
{
  enum class TouchPhase
  {
    Down,
    Moved,
    Up
  };

  struct TouchContact
  {
    uint32_t Id = 0;
    Point Position;
    TouchPhase Phase = TouchPhase::Moved;
  };

  //Touch input in session pixels: each frame has all contacts, the new ones down and the lifted ones up.
  class ITouchInjector
  {
  public:
    virtual ~ITouchInjector() = default;

    virtual void Inject(std::span<const TouchContact> frame) = 0;
  };
}
