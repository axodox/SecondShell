#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "ProductInfo.h"

using namespace ::SecondShell::Environment;

namespace winrt::SecondShell::implementation
{
  MainWindow::MainWindow()
  {
    InitializeComponent();
    VersionText().Text(to_hstring(std::format("{} {}", ProductInfo::Name(), ProductInfo::Version())));
  }

  void MainWindow::ShowSessionState(::SecondShell::Protocol::SessionState state)
  {
    StatusText().Text(to_hstring(std::format("Session: {}", Axodox::Infrastructure::to_string(state))));
  }
}