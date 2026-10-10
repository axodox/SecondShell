#pragma once
#include "MainWindow.g.h"

namespace winrt::RdpHost::implementation
{
  struct MainWindow : MainWindowT<MainWindow>
  {
    MainWindow();

    void AppendLog(const std::string& line);
  };
}

namespace winrt::RdpHost::factory_implementation
{
  struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
  {
  };
}
