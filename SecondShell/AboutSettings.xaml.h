#pragma once
#include "AboutSettings.g.h"
#include "AppEnvironment.h"

namespace winrt::SecondShell::implementation
{
  //Settings, About: the name and version, the log folder and the MCP server's path.
  struct AboutSettings : AboutSettingsT<AboutSettings>
  {
    AboutSettings();

    //On the UI thread, once.
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    void OnOpenLogFolderClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnCopyMcpPathClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    std::shared_ptr<::SecondShell::Environment::AppEnvironment> _environment;
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct AboutSettings : AboutSettingsT<AboutSettings, implementation::AboutSettings>
  {
  };
}
