#include "pch.h"
#include "ActionLogPanel.xaml.h"
#if __has_include("ActionLogPanel.g.cpp")
#include "ActionLogPanel.g.cpp"
#endif
#include "SequenceView.h"

using namespace ::SecondShell::Actions;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Ui;
using namespace std;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  //Building every group is slow beyond a few dozen
  const size_t _shownSequences = 50;

  namespace
  {
    string Lowercase(string text)
    {
      ranges::transform(text, text.begin(), [](char character) { return char(tolower(character)); });
      return text;
    }
  }

  ActionLogPanel::ActionLogPanel()
  {
    InitializeComponent();
  }

  void ActionLogPanel::Show(vector<SequenceLogEntry> sequences)
  {
    _sequences = move(sequences);
    Rebuild();
  }

  void ActionLogPanel::OnFilterClick(const IInspectable& sender, const RoutedEventArgs& /*eventArgs*/)
  {
    //One filter at a time, as a segmented control
    for (auto filter : { AllFilter(), FailedFilter(), CapturesFilter(), CommandsFilter() }) filter.IsChecked(filter == sender);
    Rebuild();
  }

  void ActionLogPanel::OnSearchChanged(const IInspectable& /*sender*/, const TextChangedEventArgs& /*eventArgs*/)
  {
    Rebuild();
  }

  void ActionLogPanel::OnExpandClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _isExpanded = !_isExpanded;
    ExpandIcon().Glyph(_isExpanded ? L"\xE73F" : L"\xE740");
    if (ExpandChanged) ExpandChanged(_isExpanded);
  }

  void ActionLogPanel::OnCloseClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    if (CloseRequested) CloseRequested();
  }

  void ActionLogPanel::Rebuild()
  {
    auto groups = Groups().Children();
    groups.Clear();

    size_t shown = 0;
    for (auto& sequence : _sequences | views::reverse)
    {
      if (shown == _shownSequences) break;

      auto group = SequenceView::Build(sequence, [&](const ActionLogEntry& step) { return Matches(sequence, step); });
      if (!group) continue;

      groups.Append(group);
      shown++;
    }

    if (shown == 0) groups.Append(EmptyText());
  }

  bool ActionLogPanel::Matches(const SequenceLogEntry& sequence, const ActionLogEntry& step)
  {
    if (FailedFilter().IsChecked().Value() && step.Status != ActionStatus::Failed) return false;
    if (CapturesFilter().IsChecked().Value() && !step.Capture) return false;
    if (CommandsFilter().IsChecked().Value() && !step.Process) return false;

    auto search = Lowercase(to_string(SearchBox().Text()));
    return search.empty() || Lowercase(step.Summary).find(search) != string::npos || Lowercase(SequenceView::Title(sequence)).find(search) != string::npos;
  }
}
