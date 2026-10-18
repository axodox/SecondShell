#pragma once
#include "GeneralSettings.g.h"
#include "CredentialStore.h"
#include "ElevatedSetupLauncher.h"
#include "SettingsStore.h"
#include "SetupChecker.h"
#include "Uninstaller.h"

namespace winrt::SecondShell::implementation
{
  //Settings, General (docs/ui/Settings.html): each change is saved at once; sign-in and setup act at once.
  struct GeneralSettings : GeneralSettingsT<GeneralSettings>
  {
    GeneralSettings();

    //On the UI thread, once.
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    //Shows the settings, the saved password and the setup as they are now.
    void Refresh();

    void OnChanged(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs& eventArgs);
    void OnInlineLimitChanged(const Microsoft::UI::Xaml::Controls::NumberBox& sender, const Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs& eventArgs);
    void OnOpenCaptureFolderClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnForgetPasswordClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnSetupClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnSmoothViewToggled(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnUninstallClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    std::shared_ptr<::SecondShell::Session::SettingsStore> _settings;
    std::shared_ptr<::SecondShell::Session::CredentialStore> _credentials;
    std::shared_ptr<::SecondShell::Setup::SetupChecker> _setupChecker;
    std::shared_ptr<::SecondShell::Setup::ElevatedSetupLauncher> _setupLauncher;
    std::shared_ptr<::SecondShell::Environment::AppEnvironment> _environment;
    std::shared_ptr<::SecondShell::Setup::Uninstaller> _uninstaller;

    std::vector<::SecondShell::Protocol::DisplaySettings> _resolutions;
    std::vector<uint32_t> _scales;
    std::vector<uint32_t> _timeouts;
    std::vector<uint32_t> _retentions;

    bool _isShowing = false;  //filling the controls, which is not a change
    bool _isSettingUp = false;

    void Save();
    void ShowPassword();
    void ShowSetup();

    //The elevated setup, then the setup shown as it is.
    fire_and_forget RunSetup(::SecondShell::Setup::SetupRequest request);

    //Asks what to remove, removes it, and says what it did (Design.md, Uninstall).
    fire_and_forget Uninstall();
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct GeneralSettings : GeneralSettingsT<GeneralSettings, implementation::GeneralSettings>
  {
  };
}
