#pragma once
#include "AiAppsSettings.g.h"
#include "IntegrationManager.h"

namespace winrt::SecondShell::implementation
{
  //Settings, Add to AI apps: the apps SecondShell knows, each with Add, Update or Remove behind the integration preview.
  struct AiAppsSettings : AiAppsSettingsT<AiAppsSettings>
  {
    AiAppsSettings();

    //On the UI thread, once.
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    //The apps as they are now.
    void Refresh();

    void OnCopySnippetClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    std::shared_ptr<::SecondShell::Integrations::IntegrationManager> _integrations;
    std::unordered_map<std::string, std::string> _problems;  //by app: why its last change did not happen
    bool _isChanging = false;

    Microsoft::UI::Xaml::Controls::Grid Card(const std::shared_ptr<::SecondShell::Integrations::AiClientIntegration>& integration, ::SecondShell::Integrations::IntegrationState state);

    //Previews the change, makes it if the user agrees, and shows the apps again.
    fire_and_forget Change(std::shared_ptr<::SecondShell::Integrations::AiClientIntegration> integration, bool isAdding);
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct AiAppsSettings : AiAppsSettingsT<AiAppsSettings, implementation::AiAppsSettings>
  {
  };
}
