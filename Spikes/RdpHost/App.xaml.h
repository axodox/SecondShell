#pragma once
#include "App.xaml.g.h"
#include "SpikeRunner.h"

namespace winrt::RdpHost::implementation
{
  struct App : AppT<App>
  {
    App();

    void OnLaunched(const Microsoft::UI::Xaml::LaunchActivatedEventArgs& eventArgs);

  private:
    Microsoft::UI::Xaml::Window _window{ nullptr };
    std::unique_ptr<SpikeRunner> _runner;
  };
}
