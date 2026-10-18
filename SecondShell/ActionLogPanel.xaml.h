#pragma once
#include "ActionLogPanel.g.h"
#include "ActionLog.h"

namespace winrt::SecondShell::implementation
{
  //The action log's content: one group per sequence, newest first, filtered and searched. The status bar shows it in
  //its flyout.
  struct ActionLogPanel : ActionLogPanelT<ActionLogPanel>
  {
    ActionLogPanel();

    void Show(std::vector<::SecondShell::Agent::SequenceLogEntry> sequences);

    std::function<void()> CloseRequested;
    std::function<void(bool isExpanded)> ExpandChanged;

    void OnFilterClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnSearchChanged(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::TextChangedEventArgs& eventArgs);
    void OnExpandClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnCloseClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    std::vector<::SecondShell::Agent::SequenceLogEntry> _sequences;
    bool _isExpanded = false;

    void Rebuild();
    bool Matches(const ::SecondShell::Agent::SequenceLogEntry& sequence, const ::SecondShell::Agent::ActionLogEntry& step);
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct ActionLogPanel : ActionLogPanelT<ActionLogPanel, implementation::ActionLogPanel>
  {
  };
}
