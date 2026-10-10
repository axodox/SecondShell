#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <microsoft.ui.xaml.window.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::RdpHost::implementation
{
  App::App()
  {
  }

  void App::OnLaunched(const LaunchActivatedEventArgs& /*eventArgs*/)
  {
    auto window = make<MainWindow>();
    _window = window;

    //Shown without activation: the host must never take focus from the user.
    HWND handle = nullptr;
    _window.as<IWindowNative>()->get_WindowHandle(&handle);
    _window.AppWindow().MoveAndResize({ 40, 40, 900, 500 });
    _window.AppWindow().Show(false);

    auto options = SpikeOptions::Parse();
    _runner = std::make_unique<SpikeRunner>(options, _window.DispatcherQueue(), [window](const std::string& line) {
      get_self<MainWindow>(window)->AppendLog(line);
    });
    _runner->Start(handle);
  }
}
