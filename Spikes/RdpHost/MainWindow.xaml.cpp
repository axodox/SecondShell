#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace winrt::RdpHost::implementation
{
  MainWindow::MainWindow()
  {
    InitializeComponent();
  }

  void MainWindow::AppendLog(const std::string& line)
  {
    LogText().Text(LogText().Text() + to_hstring(Spike::Timestamp() + " " + line + "\n"));
    LogScroller().ChangeView(nullptr, LogScroller().ScrollableHeight(), nullptr);
  }
}
