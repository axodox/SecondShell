#pragma once
#include "ScreenWord.h"
#include "WindowId.h"

namespace SecondShell::Capture
{
  //Text an application exposes for a window, without OCR (UI Automation).
  class IWindowTextSource
  {
  public:
    virtual ~IWindowTextSource() = default;

    //The window's text, with boxes on the screen; empty if it exposes none or does not answer in time.
    virtual std::vector<ScreenWord> Read(Actions::WindowId window) const = 0;
  };
}
