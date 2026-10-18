#pragma once
#include "App.xaml.g.h"
#include "MainWindow.xaml.h"
#include "SessionManager.h"
#include "SessionRenderer.h"

namespace winrt::SecondShell::implementation
{
  //The composition root of the host.
  struct App : AppT<App>
  {
    App();

    void OnLaunched(const Microsoft::UI::Xaml::LaunchActivatedEventArgs& eventArgs);

  private:
    std::unique_ptr<Axodox::Infrastructure::dependency_container> _container;
    winrt::com_ptr<MainWindow> _window;
    Axodox::Infrastructure::event_subscription _stateChangedSubscription;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    Axodox::Infrastructure::event_subscription _runningChangedSubscription;
    Axodox::Infrastructure::event_subscription _actionStartedSubscription;
    Axodox::Infrastructure::event_subscription _entryAddedSubscription;

    void RegisterServices();
    bool StartServices();
    void ShowWindow();
  };
}
