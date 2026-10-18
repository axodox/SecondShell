#include "pch.h"
#include "ScreenPosition.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Input;
using namespace std;

namespace SecondShell::Driver
{
  value_ptr<ActionResult> CheckOnScreen(Point position, Point screenSize)
  {
    if (position.X >= 0 && position.Y >= 0 && position.X < screenSize.X && position.Y < screenSize.Y) return nullptr;

    return ActionResult::Failed(ErrorCode::InvalidArgument, format("({}, {}) is outside the screen, which is {}x{} pixels.", position.X, position.Y, screenSize.X, screenSize.Y));
  }
}
