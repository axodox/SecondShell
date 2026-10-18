#pragma once
#include "App.xaml.g.h"
#include "MainWindow.xaml.h"
#include "ElevatedSetupLauncher.h"
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

    //SecondShell.exe --setup, started elevated by ElevatedSetupLauncher: the machine-wide steps, then the exit code.
    static int RunSetup(const ::SecondShell::Setup::SetupRequest& request);

    void RegisterServices();
    bool StartServices();
    void ShowWindow();
  };
}
