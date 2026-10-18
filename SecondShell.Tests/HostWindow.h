#pragma once

namespace SecondShell::Tests
{
  //The running host's window, for tests which use it as the user would: through its placement and its buttons.
  class HostWindow
  {
  public:
    //Fails the test if no host window is open.
    HostWindow();

    HWND Handle() const;

    //Invokes the button of the window with this accessible name, e.g. "Stop", as a click would.
    void Click(std::wstring_view name) const;

  private:
    HWND _handle = nullptr;
  };
}
