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

    //SecondShell.exe --integrate <app> / --unintegrate <app>: adds SecondShell to an AI app, or removes it, without the
    //window (Design.md, AI client integration); writes what it did to the console it was started from.
    static std::optional<int> RunIntegrationCommand(std::span<const std::wstring> arguments);

    //SecondShell.exe --uninstall [--forget-password] [--delete-data]: removes what SecondShell added (Design.md,
    //Uninstall); turning the smooth session view off asks for administrator rights.
    static std::optional<int> RunUninstall(std::span<const std::wstring> arguments);

    //Writes to the console the command line was started from, if there is one.
    static void UseParentConsole();

    void RegisterServices();
    bool StartServices();
    void ShowWindow();
  };
}
