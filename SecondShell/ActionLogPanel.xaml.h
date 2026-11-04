#pragma once
#include "ActionLogPanel.g.h"
#include "ActionLog.h"

namespace winrt::SecondShell::implementation
{
  //The action log's content: one group per sequence, newest first, filtered and searched. The status bar shows it in
  //its flyout. The headers and steps of the groups are rows of a repeater, which builds only those on the screen; an
  //update changes only the rows which changed, so a log of thousands of steps stays quick.
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
    void OnRowPrepared(const Microsoft::UI::Xaml::Controls::ItemsRepeater& sender, const Microsoft::UI::Xaml::Controls::ItemsRepeaterElementPreparedEventArgs& eventArgs);
    void OnRowClearing(const Microsoft::UI::Xaml::Controls::ItemsRepeater& sender, const Microsoft::UI::Xaml::Controls::ItemsRepeaterElementClearingEventArgs& eventArgs);

  private:
    //A sequence's header (no step) or one of its steps
    struct Row
    {
      std::string Key;        //which row it is, across updates
      std::string Signature;  //what it shows: a row whose signature changed is built again
      size_t Sequence = 0;    //in _sequences
      std::optional<size_t> Step;
      bool IsEdge = false;    //the first header of the log, or the last step shown of a sequence
    };

    std::vector<::SecondShell::Agent::SequenceLogEntry> _sequences;
    std::vector<Row> _rows;
    Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> _items;
    bool _isExpanded = false;

    void Rebuild();
    void Apply(std::vector<Row>&& rows);
    bool Matches(std::string_view title, std::string_view search, const ::SecondShell::Agent::ActionLogEntry& step);
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct ActionLogPanel : ActionLogPanelT<ActionLogPanel, implementation::ActionLogPanel>
  {
  };
}
