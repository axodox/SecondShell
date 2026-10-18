#pragma once
#include "ActionEnums.h"
#include "Point.h"

namespace SecondShell::Input
{
  //Mouse input in session pixels.
  class IMouseInjector
  {
  public:
    virtual ~IMouseInjector() = default;

    virtual Point Position() const = 0;
    virtual Point ScreenSize() const = 0;

    virtual void MoveTo(Point position) = 0;
    virtual void Press(Actions::MouseButton button) = 0;
    virtual void Release(Actions::MouseButton button) = 0;
    virtual void Wheel(float notches, Actions::WheelAxis axis) = 0;
  };
}
