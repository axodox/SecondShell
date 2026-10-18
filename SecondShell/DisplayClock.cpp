#include "pch.h"
#include "DisplayClock.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  const milliseconds _fallbackInterval{ 4 };

  DisplayClock::DisplayClock(HWND window) :
    _window(window)
  { }

  void DisplayClock::Wait()
  {
    //The window may have moved to another monitor
    auto monitor = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
    if (monitor != _monitor) FindOutput(monitor);

    if (!_output || FAILED(_output->WaitForVBlank())) this_thread::sleep_for(_fallbackInterval);
  }

  void DisplayClock::FindOutput(HMONITOR monitor)
  {
    _monitor = monitor;
    _output = nullptr;

    //The monitor's output may be on another adapter than the one drawing
    winrt::com_ptr<IDXGIFactory1> factory;
    winrt::check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(factory.put())));

    winrt::com_ptr<IDXGIAdapter1> adapter;
    for (uint32_t adapterIndex = 0; factory->EnumAdapters1(adapterIndex, adapter.put()) == S_OK; adapterIndex++, adapter = nullptr)
    {
      winrt::com_ptr<IDXGIOutput> output;
      for (uint32_t outputIndex = 0; adapter->EnumOutputs(outputIndex, output.put()) == S_OK; outputIndex++, output = nullptr)
      {
        DXGI_OUTPUT_DESC description;
        if (SUCCEEDED(output->GetDesc(&description)) && description.Monitor == monitor)
        {
          _output = output;
          return;
        }
      }
    }

    _logger.log(log_severity::warning, "No display output found for the window's monitor; drawing on a timer.");
  }
}
