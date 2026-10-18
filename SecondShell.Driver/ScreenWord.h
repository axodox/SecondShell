#pragma once
#include "Rect.h"

namespace SecondShell::Capture
{
  //A word read from the screen, or a run of words where the source gives no finer boxes (UI Automation).
  struct ScreenWord
  {
    std::string Text;
    Rect Box;  //on the screen
  };
}
