#include "pch.h"
#include "ProcessesPanel.xaml.h"
#if __has_include("ProcessesPanel.g.cpp")
#include "ProcessesPanel.g.cpp"
#endif
#include "ActionView.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>

using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Ui;
using namespace std;
using namespace std::chrono;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::SecondShell::implementation
{
  //In UTF-8, whatever the compiler's execution character set
  const char* const _separator = " \xC2\xB7 ";

  namespace
  {
    string State(const ProcessOutputInfo& process)
    {
      if (process.IsRunning) return "running";
      return process.ExitCode ? std::format("exited with {}", *process.ExitCode) : string("exited");
    }

    string LocalTime(system_clock::time_point time)
    {
      try
      {
        return std::format("{:%Y-%m-%d %H:%M}", zoned_time{ current_zone(), floor<minutes>(time) });
      }
      catch (const exception&)
      {
        return std::format("{:%Y-%m-%d %H:%M} UTC", floor<minutes>(time));
      }
    }

    string Size(uint64_t bytes)
    {
      return bytes < 1024 * 1024 ? std::format("{} KB", (bytes + 1023) / 1024) : std::format("{:.1f} MB", bytes / (1024.0 * 1024.0));
    }

    string Lowercase(string text)
    {
      ranges::transform(text, text.begin(), [](char character) { return char(tolower(uint8_t(character))); });
      return text;
    }

    hstring LineText(const IInspectable& item)
    {
      return unbox_value_or<hstring>(item, L"");
    }

    ListViewItem Row(string_view title, string_view detail, const wchar_t* detailBrush, string_view key)
    {
      auto titleText = ActionView::Text(title, {}, 12);
      titleText.TextTrimming(TextTrimming::CharacterEllipsis);
      titleText.FontFamily(ActionView::Resource<Media::FontFamily>(L"MonoFontFamily"));

      //The key on both: a click reports the content
      StackPanel content;
      content.Spacing(2);
      content.Padding({ 0, 4, 0, 4 });
      content.Tag(box_value(to_hstring(key)));
      content.Children().Append(titleText);
      content.Children().Append(ActionView::Text(detail, detailBrush, 11));

      ListViewItem item;
      item.Content(content);
      item.Tag(box_value(to_hstring(key)));
      ToolTipService::SetToolTip(item, box_value(to_hstring(title)));
      return item;
    }
  }

  ProcessesPanel::ProcessesPanel() :
    _lines(single_threaded_observable_vector<IInspectable>())
  {
    InitializeComponent();
    Lines().ItemsSource(_lines);
  }

  void ProcessesPanel::UseStore(shared_ptr<ProcessOutputStore> store, filesystem::path outputFolder)
  {
    _store = move(store);
    _outputFolder = move(outputFolder);
  }

  void ProcessesPanel::Refresh(bool isHistoryChecked)
  {
    if (!_store) return;

    auto version = _store->Version();
    if (version == _version && !isHistoryChecked) return;
    _version = version;

    if (isHistoryChecked) _history = ProcessOutputStore::History(_outputFolder);
    ShowEntries();
    auto selected = Selected();
    if (selected && selected->Live) ShowLines(false);
    ShowDetail();
  }

  void ProcessesPanel::OnFilterClick(const IInspectable& sender, const RoutedEventArgs& /*eventArgs*/)
  {
    //One filter at a time, as a segmented control
    for (auto filter : { AllFilter(), RunningFilter(), FinishedFilter() }) filter.IsChecked(filter == sender);
    ShowEntries(true);
    ShowDetail();
  }

  void ProcessesPanel::OnSearchChanged(const IInspectable& /*sender*/, const TextChangedEventArgs& /*eventArgs*/)
  {
    ShowEntries(true);
    ShowDetail();
  }

  bool ProcessesPanel::Matches(const Entry& entry, string_view search)
  {
    auto isRunning = entry.Live && entry.Live->IsRunning;
    if (RunningFilter().IsChecked().Value() && !isRunning) return false;
    if (FinishedFilter().IsChecked().Value() && isRunning) return false;

    if (search.empty()) return true;
    auto text = entry.Live ? std::format("{} {}", entry.Live->Command, entry.Live->ProcessId) : std::format("{} {}", entry.File->Program, entry.File->ProcessId);
    return Lowercase(text).find(search) != string::npos;
  }

  void ProcessesPanel::ShowEntries(bool isForced)
  {
    //This run's processes, then the files of the others, as the filter and the search let through
    auto search = Lowercase(to_string(SearchBox().Text()));
    vector<Entry> entries;
    for (auto& process : _store->Processes())
    {
      Entry entry{ process.OutputFile, process, nullopt };
      if (Matches(entry, search)) entries.push_back(move(entry));
    }
    auto liveCount = entries.size();

    for (auto& file : _history)
    {
      Entry entry{ winrt::to_string(file.File.wstring()), nullopt, file };
      auto isListed = ranges::any_of(entries, [&](auto& item) { return item.Key == entry.Key; }) || ranges::any_of(_store->Processes(), [&](auto& process) { return process.OutputFile == entry.Key; });
      if (!isListed && Matches(entry, search)) entries.push_back(move(entry));
    }
    EmptyText().Visibility(entries.empty() ? Visibility::Visible : Visibility::Collapsed);
    EmptyText().Text(_store->Processes().empty() && _history.empty() ?
      L"No processes yet. The agent starts one here with Process.Run, async and output, e.g. a server whose log it reads." :
      L"No process matches.");

    //Rebuilt only when a process came, went, or ended, or the filter changed
    auto isSame = !isForced && entries.size() == _entries.size() && ranges::equal(entries, _entries, [](auto& left, auto& right) {
      return left.Key == right.Key && (left.Live ? left.Live->IsRunning : false) == (right.Live ? right.Live->IsRunning : false) && (left.Live ? left.Live->ExitCode : nullopt) == (right.Live ? right.Live->ExitCode : nullopt);
      });
    _entries = move(entries);
    if (isSame) return;

    auto items = ProcessList().Items();
    items.Clear();
    auto isSelectedShown = false;
    for (size_t index = 0; index < _entries.size(); index++)
    {
      auto& entry = _entries[index];

      //A heading before the earlier ones, below this run's
      if (index == liveCount && liveCount > 0)
      {
        ListViewItem heading;
        heading.Content(ActionView::Text("Earlier", L"MutedTextBrush", 11));
        heading.IsEnabled(false);
        heading.IsHitTestVisible(false);
        items.Append(heading);
      }

      if (_selected == entry.Key) isSelectedShown = true;
      if (entry.Live)
      {
        auto& process = *entry.Live;
        items.Append(Row(process.Command, std::format("PID {}{}{}", process.ProcessId, _separator, State(process)), process.IsRunning ? L"RunningBadgeTextBrush" : L"MutedTextBrush", entry.Key));
      }
      else
      {
        auto& file = *entry.File;
        items.Append(Row(file.Program, std::format("PID {}{}{}{}{}", file.ProcessId, _separator, LocalTime(file.Started), _separator, Size(file.Size)), L"MutedTextBrush", entry.Key));
      }
    }

    //The newest shown, until the user picks one
    if (!isSelectedShown && !_entries.empty())
    {
      Select(_entries.front().Key);
      return;
    }

    MarkSelected();
    if (_entries.empty()) ShowLines(true);
  }

  void ProcessesPanel::Select(const string& key)
  {
    _selected = key;
    MarkSelected();
    ShowLines(true);
    ShowDetail();
  }

  void ProcessesPanel::MarkSelected()
  {
    auto selected = to_hstring(_selected.value_or(""));
    for (auto item : ProcessList().Items())
    {
      auto row = item.try_as<ListViewItem>();
      if (!row || !row.Tag()) continue;

      auto isSelected = unbox_value<hstring>(row.Tag()) == selected;
      if (isSelected) row.Background(ActionView::Resource<Media::Brush>(L"RunningStepBrush"));
      else row.ClearValue(Control::BackgroundProperty());
      Microsoft::UI::Xaml::Automation::AutomationProperties::SetItemStatus(row, isSelected ? L"Selected" : L"");
    }
  }

  void ProcessesPanel::ShowLines(bool isReset)
  {
    auto entry = Selected();
    if (!entry)
    {
      _lines.Clear();
      return;
    }

    //An earlier process: the end of its file, once
    if (entry->File)
    {
      if (!isReset) return;

      vector<IInspectable> items;
      for (auto& line : ProcessOutputStore::ReadTail(*entry->File)) items.push_back(box_value(to_hstring(line)));
      _lines.ReplaceAll(items);
      _firstLine = 0;
      return;
    }

    //From the last line shown, which may have grown
    auto processId = entry->Live->ProcessId;
    auto from = isReset || _lines.Size() == 0 ? 0 : _firstLine + _lines.Size() - 1;
    auto chunk = _store->Lines(processId, from);

    //The store keeps the last lines only; what it dropped goes here too
    while (!isReset && _lines.Size() > 0 && _firstLine < chunk.Oldest)
    {
      _lines.RemoveAt(0);
      _firstLine++;
    }

    auto end = _firstLine + _lines.Size();
    if (isReset || _lines.Size() == 0 || chunk.First > end)
    {
      vector<IInspectable> items;
      items.reserve(chunk.Lines.size());
      for (auto& line : chunk.Lines) items.push_back(box_value(to_hstring(line)));
      _lines.ReplaceAll(items);
      _firstLine = chunk.First;
      return;
    }

    for (size_t index = 0; index < chunk.Lines.size(); index++)
    {
      auto number = chunk.First + index;
      if (number + 1 < end) continue;

      auto text = to_hstring(chunk.Lines[index]);
      if (number + 1 == end)
      {
        if (LineText(_lines.GetAt(_lines.Size() - 1)) != text) _lines.SetAt(_lines.Size() - 1, box_value(text));
      }
      else
      {
        _lines.Append(box_value(text));
      }
    }
  }

  void ProcessesPanel::ShowDetail()
  {
    auto entry = Selected();
    CopyButton().IsEnabled(entry && _lines.Size() > 0);

    string detail;
    filesystem::path file;
    if (entry && entry->Live)
    {
      detail = std::format("PID {}{}{}{}{} lines", entry->Live->ProcessId, _separator, State(*entry->Live), _separator, entry->Live->LineCount);
      file = to_hstring(entry->Live->OutputFile).c_str();
    }
    else if (entry && entry->File)
    {
      auto parts = entry->File->Parts.size();
      detail = std::format("PID {}{}started {}{}{}{}{}its last {} lines", entry->File->ProcessId, _separator, LocalTime(entry->File->Started), _separator,
        parts > 1 ? std::format("{} files, ", parts) : "", Size(entry->File->Size), _separator, _lines.Size());
      file = entry->File->Parts.empty() ? entry->File->File : entry->File->Parts.back();
    }

    DetailText().Text(to_hstring(detail));
    error_code error;
    OpenFileButton().IsEnabled(!file.empty() && filesystem::exists(file, error));
  }

  const ProcessesPanel::Entry* ProcessesPanel::Selected() const
  {
    if (!_selected) return nullptr;
    auto entry = ranges::find(_entries, *_selected, &Entry::Key);
    return entry == _entries.end() ? nullptr : &*entry;
  }

  void ProcessesPanel::OnProcessClick(const IInspectable& /*sender*/, const ItemClickEventArgs& eventArgs)
  {
    auto element = eventArgs.ClickedItem().try_as<FrameworkElement>();
    if (!element || !element.Tag()) return;

    auto key = to_string(unbox_value<hstring>(element.Tag()));
    if (key != _selected) Select(key);
  }

  void ProcessesPanel::OnCopyClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    wstring text;
    for (auto line : _lines)
    {
      text += LineText(line);
      text += L"\r\n";
    }

    DataPackage package;
    package.SetText(text);
    Clipboard::SetContent(package);
  }

  void ProcessesPanel::OnOpenFileClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    auto entry = Selected();
    if (!entry) return;

    auto file = entry->Live ? filesystem::path(to_hstring(entry->Live->OutputFile).c_str()) : entry->File->Parts.empty() ? entry->File->File : entry->File->Parts.back();
    ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }

  void ProcessesPanel::OnExpandClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _isExpanded = !_isExpanded;
    ExpandIcon().Glyph(_isExpanded ? L"\xE73F" : L"\xE740");
    if (ExpandChanged) ExpandChanged(_isExpanded);
  }

  void ProcessesPanel::OnCloseClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    if (CloseRequested) CloseRequested();
  }
}
