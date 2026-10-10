#pragma once
#include "MainWindow.g.h"

namespace winrt::SecondShell::implementation
{
  struct MainWindow : MainWindowT<MainWindow>
  {
    MainWindow();
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
  {
  };
}