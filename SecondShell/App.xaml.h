#pragma once
#include "App.xaml.g.h"

namespace winrt::SecondShell::implementation
{
  struct App : AppT<App>
  {
    App();

    void OnLaunched(const Microsoft::UI::Xaml::LaunchActivatedEventArgs& eventArgs);

  private:
    Microsoft::UI::Xaml::Window _window{ nullptr };
  };
}