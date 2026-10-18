#include "pch.h"
#include "SettingsView.xaml.h"
#if __has_include("SettingsView.g.cpp")
#include "SettingsView.g.cpp"
#endif
#include "AboutSettings.xaml.h"
#include "GeneralSettings.xaml.h"

using namespace Axodox::Infrastructure;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::SecondShell::implementation
{
  SettingsView::SettingsView()
  {
    InitializeComponent();
  }

  void SettingsView::UseServices(dependency_container* container)
  {
    get_self<GeneralSettings>(GeneralPage())->UseServices(container);
    get_self<AboutSettings>(AboutPage())->UseServices(container);
  }

  void SettingsView::Refresh()
  {
    get_self<GeneralSettings>(GeneralPage())->Refresh();
    PageScroller().ChangeView(nullptr, 0.0, nullptr, true);
  }

  void SettingsView::OnPageSelected(const NavigationView& /*sender*/, const NavigationViewSelectionChangedEventArgs& eventArgs)
  {
    auto item = eventArgs.SelectedItem().try_as<NavigationViewItem>();
    auto page = item ? unbox_value_or<hstring>(item.Tag(), L"General") : hstring{ L"General" };

    GeneralPage().Visibility(page == L"General" ? Visibility::Visible : Visibility::Collapsed);
    AboutPage().Visibility(page == L"About" ? Visibility::Visible : Visibility::Collapsed);
    PageScroller().ChangeView(nullptr, 0.0, nullptr, true);
  }
}
