#pragma once
#include "SettingsView.g.h"

namespace winrt::SecondShell::implementation
{
  //The settings pages, in place of the session view while open (docs/UIDesign.md, Settings).
  struct SettingsView : SettingsViewT<SettingsView>
  {
    SettingsView();

    //On the UI thread, once.
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    //Shows the page again from its top, with what changed since.
    void Refresh();

    void OnPageSelected(const Microsoft::UI::Xaml::Controls::NavigationView& sender, const Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs& eventArgs);
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct SettingsView : SettingsViewT<SettingsView, implementation::SettingsView>
  {
  };
}
