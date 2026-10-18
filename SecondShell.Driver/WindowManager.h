#pragma once
#include "ActionValues.h"

namespace SecondShell::Desktop
{
  //The top-level windows of the session, as the taskbar lists them. Positions and sizes are of the visible frame,
  //without the invisible resize borders, as captures show it. Changes are requested asynchronously and then awaited
  //briefly, so a hung application cannot block the driver.
  class WindowManager
  {
  public:
    std::vector<Actions::WindowInfo> Enumerate() const;

    //Throws invalid_argument if there is no such visible window.
    Actions::WindowInfo Get(Actions::WindowId window) const;
    std::optional<Actions::WindowId> Foreground() const;

    //False if Windows refused to switch the foreground window.
    bool Focus(Actions::WindowId window);
    void Move(Actions::WindowId window, int32_t x, int32_t y);
    void Resize(Actions::WindowId window, uint32_t width, uint32_t height);

    //False if the window did not reach the state, e.g. a window which cannot be maximized.
    bool SetState(Actions::WindowId window, Actions::WindowState state);

    //A close request, as when clicking the X button; the application may ask for confirmation first.
    void Close(Actions::WindowId window);

  private:
    HWND Handle(Actions::WindowId window) const;
    void SetBounds(HWND handle, const RECT& frame);
  };
}
