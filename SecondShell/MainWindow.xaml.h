#pragma once
#include "MainWindow.g.h"
#include "HostProtocol.h"

namespace winrt::SecondShell::implementation
{
  struct MainWindow : MainWindowT<MainWindow>
  {
    MainWindow();

    void ShowSessionState(::SecondShell::Protocol::SessionState state);
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
  {
  };
}