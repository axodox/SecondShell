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
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::SecondShell::implementation
{
  namespace
  {
    string Lowercase(string text)
    {
      ranges::transform(text, text.begin(), [](char character) { return char(tolower(character)); });
      return text;
    }

    //Sequences are numbered by the host run; actions outside a sequence share number 0, so the time tells them apart
    string SequenceKey(const SequenceLogEntry& sequence)
    {
      return std::format("{}@{}", sequence.Number, sequence.Time.time_since_epoch().count());
    }

    //What a header shows: a running sequence's duration changes with its steps, as before
    string HeaderSignature(const SequenceLogEntry& sequence, bool isFirst)
    {
      return std::format("{}|{}|{}|{}|{}", isFirst, sequence.Steps.size(), sequence.StepCount, sequence.Duration ? sequence.Duration->count() : -1, sequence.Error ? int(*sequence.Error) : -1);
    }

    string StepSignature(const ActionLogEntry& step, bool isLast)
    {
      return std::format("{}|{}|{}|{}|{}|{}|{}", isLast, step.Status ? int(*step.Status) : -1, step.Duration ? step.Duration->count() : -1, step.Message.size(),
        step.Capture ? step.Capture->File : "", step.Process ? step.Process->Output.size() : 0, step.Process && step.Process->ExitCode ? *step.Process->ExitCode : INT32_MIN);
    }
  }

  ActionLogPanel::ActionLogPanel() :
    _items(single_threaded_observable_vector<IInspectable>())
  {
    InitializeComponent();
    Rows().ItemsSource(_items);
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

  void ActionLogPanel::OnRowPrepared(const ItemsRepeater& /*sender*/, const ItemsRepeaterElementPreparedEventArgs& eventArgs)
  {
    auto index = size_t(eventArgs.Index());
    if (index >= _rows.size()) return;

    auto& row = _rows[index];
    auto& sequence = _sequences[row.Sequence];
    auto content = row.Step ? SequenceView::StepRow(sequence, sequence.Steps[*row.Step], row.IsEdge) : SequenceView::HeaderRow(sequence, row.IsEdge);
    eventArgs.Element().as<ContentControl>().Content(content);
  }

  void ActionLogPanel::OnRowClearing(const ItemsRepeater& /*sender*/, const ItemsRepeaterElementClearingEventArgs& eventArgs)
  {
    eventArgs.Element().as<ContentControl>().Content(nullptr);
  }

  void ActionLogPanel::Rebuild()
  {
    auto search = Lowercase(to_string(SearchBox().Text()));

    //Newest sequence first, its steps in order
    vector<Row> rows;
    for (size_t index = _sequences.size(); index-- > 0;)
    {
      auto& sequence = _sequences[index];
      auto title = Lowercase(SequenceView::Title(sequence));

      vector<size_t> steps;
      for (size_t step = 0; step < sequence.Steps.size(); step++)
      {
        if (Matches(title, search, sequence.Steps[step])) steps.push_back(step);
      }
      if (steps.empty()) continue;

      auto key = SequenceKey(sequence);
      auto isFirst = rows.empty();
      rows.push_back({ key, HeaderSignature(sequence, isFirst), index, nullopt, isFirst });
      for (auto& step : steps)
      {
        auto isLast = &step == &steps.back();
        rows.push_back({ std::format("{}:{}", key, step), StepSignature(sequence.Steps[step], isLast), index, step, isLast });
      }
    }

    EmptyText().Visibility(rows.empty() ? Visibility::Visible : Visibility::Collapsed);
    Apply(move(rows));
  }

  void ActionLogPanel::Apply(vector<Row>&& rows)
  {
    //The rows are read when the repeater lays them out, after this
    auto previous = exchange(_rows, move(rows));
    auto& current = _rows;

    //The oldest sequences leave the log at its end
    unordered_set<string_view> keys;
    for (auto& row : current) keys.insert(row.Key);
    while (!previous.empty() && !keys.contains(previous.back().Key))
    {
      previous.pop_back();
      _items.RemoveAtEnd();
    }

    //What stayed the same at both ends; the rest in between is replaced, usually just the new steps
    size_t suffix = 0;
    while (suffix < previous.size() && suffix < current.size() && previous[previous.size() - 1 - suffix].Key == current[current.size() - 1 - suffix].Key) suffix++;

    size_t prefix = 0;
    while (prefix < previous.size() - suffix && prefix < current.size() - suffix && previous[prefix].Key == current[prefix].Key) prefix++;

    for (auto index = previous.size() - suffix; index-- > prefix;) _items.RemoveAt(uint32_t(index));
    for (auto index = prefix; index < current.size() - suffix; index++) _items.InsertAt(uint32_t(index), box_value(to_hstring(current[index].Key)));

    //Rows at both ends which show something else now, e.g. a step that finished, are built again
    auto rebuildIfChanged = [&](size_t index, const Row& before) {
      if (before.Signature != current[index].Signature) _items.SetAt(uint32_t(index), box_value(to_hstring(current[index].Key)));
      };
    for (size_t index = 0; index < prefix; index++) rebuildIfChanged(index, previous[index]);
    for (size_t offset = 1; offset <= suffix; offset++) rebuildIfChanged(current.size() - offset, previous[previous.size() - offset]);
  }

  bool ActionLogPanel::Matches(string_view title, string_view search, const ActionLogEntry& step)
  {
    if (FailedFilter().IsChecked().Value() && step.Status != ActionStatus::Failed) return false;
    if (CapturesFilter().IsChecked().Value() && !step.Capture) return false;
    if (CommandsFilter().IsChecked().Value() && !step.Process) return false;

    return search.empty() || title.find(search) != string_view::npos || Lowercase(step.Summary).find(search) != string::npos;
  }
}
