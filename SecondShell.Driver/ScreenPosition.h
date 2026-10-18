#pragma once
#include "ActionResult.h"
#include "Point.h"

namespace SecondShell::Driver
{
  //A failed result if the position is outside the screen of the size given; null if it is on it.
  Axodox::Infrastructure::value_ptr<Actions::ActionResult> CheckOnScreen(Input::Point position, Input::Point screenSize);
}
