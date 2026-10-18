#include "pch.h"
#include "AboutSettings.xaml.h"
#if __has_include("AboutSettings.g.cpp")
#include "AboutSettings.g.cpp"
#endif
#include "ProductInfo.h"

using namespace ::SecondShell::Environment;
using namespace Axodox::Infrastructure;
using namespace std;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  AboutSettings::AboutSettings()
  {
    InitializeComponent();
    NameText().Text(to_hstring(std::format("{} {}", ProductInfo::Name(), ProductInfo::Version())));
  }

  void AboutSettings::UseServices(dependency_container* container)
  {
    _environment = container->resolve<AppEnvironment>();
    LogFolderText().Text(_environment->LogFolder().c_str());
    McpPathText().Text(_environment->McpExecutable().c_str());
  }

  void AboutSettings::OnOpenLogFolderClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    ShellExecuteW(nullptr, L"open", _environment->LogFolder().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }

  void AboutSettings::OnCopyMcpPathClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    DataPackage package;
    package.SetText(_environment->McpExecutable().c_str());
    Clipboard::SetContent(package);
  }
}
