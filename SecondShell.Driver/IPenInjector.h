#pragma once
#include "Point.h"

namespace SecondShell::Input
{
  //Where the pen is and how it is held. Out of range, the rest does not matter.
  struct PenState
  {
    bool IsInRange = false;
    bool IsInContact = false;
    Point Position;
    float Pressure = 0.f;  //0..1, only in contact
    int32_t TiltX = 0;     //degrees, -90..90
    int32_t TiltY = 0;
    uint32_t Rotation = 0; //degrees, 0..359
    bool IsBarrelPressed = false;
    bool IsEraser = false;

    bool operator==(const PenState&) const = default;
  };

  //Pen input in session pixels: each state replaces the previous one; the injector works out what changed.
  class IPenInjector
  {
  public:
    virtual ~IPenInjector() = default;

    virtual void Inject(const PenState& state) = 0;
  };
}
