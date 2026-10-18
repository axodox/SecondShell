#pragma once

namespace SecondShell::Rendering
{
  //The refresh of the monitor a window is on, so drawing keeps in step with it, at any refresh rate.
  class DisplayClock
  {
    inline static const Axodox::Infrastructure::logger _logger{ "DisplayClock" };

  public:
    explicit DisplayClock(HWND window);

    //Until the monitor's next vertical blank; a short sleep where there is no monitor to wait for.
    void Wait();

  private:
    HWND _window;
    HMONITOR _monitor = nullptr;
    winrt::com_ptr<IDXGIOutput> _output;

    void FindOutput(HMONITOR monitor);
  };
}
