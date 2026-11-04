#include "pch.h"
#include "ProcessesPanel.xaml.h"
#if __has_include("ProcessesPanel.g.cpp")
#include "ProcessesPanel.g.cpp"
#endif
#include "ActionView.h"
#include <winrt/Microsoft.UI.Xaml.Documents.h>
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

    if (isHistoryChecked)
    {
      _history = ProcessOutputStore::History(_outputFolder);
      erase_if(_history, [this](auto& file) { return file.Started < _since; });
    }
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
      SetLines({});
      return;
    }

    //An earlier process: the end of its file, once
    if (entry->File)
    {
      if (isReset) SetLines(ProcessOutputStore::ReadTail(*entry->File));
      _firstLine = 0;
      return;
    }

    //From the last line shown, which may have grown
    auto processId = entry->Live->ProcessId;
    auto from = isReset || _rawLines.empty() ? 0 : _firstLine + _rawLines.size() - 1;
    auto chunk = _store->Lines(processId, from);

    //The store keeps the last lines only; what it dropped goes here too
    while (!isReset && !_rawLines.empty() && _firstLine < chunk.Oldest)
    {
      _lines.RemoveAt(0);
      _rawLines.pop_front();
      _lineStyles.pop_front();
      _firstLine++;
    }

    auto end = _firstLine + _rawLines.size();
    if (isReset || _rawLines.empty() || chunk.First > end)
    {
      SetLines(chunk.Lines);
      _firstLine = chunk.First;
      return;
    }

    for (size_t index = 0; index < chunk.Lines.size(); index++)
    {
      auto number = chunk.First + index;
      if (number + 1 < end) continue;

      auto& line = chunk.Lines[index];
      if (number + 1 == end)
      {
        if (_rawLines.back() == line) continue;

        //It grew: its colors from the same start
        _rawLines.back() = line;
        _nextStyle = NextStyle(_lineStyles.back(), line);
        _lines.SetAt(_lines.Size() - 1, box_value(to_hstring(line)));
      }
      else
      {
        _lineStyles.push_back(_nextStyle);
        _nextStyle = NextStyle(_nextStyle, line);
        _rawLines.push_back(line);
        _lines.Append(box_value(to_hstring(line)));
      }
    }
  }

  ::SecondShell::Text::TextStyle ProcessesPanel::NextStyle(const ::SecondShell::Text::TextStyle& start, const string& line)
  {
    ::SecondShell::Text::AnsiParser parser{ start };
    parser.Parse(line);
    return parser.Style();
  }

  void ProcessesPanel::SetLines(const vector<string>& lines)
  {
    //Each line's colors start where the previous one's ended, as in a terminal
    _rawLines.assign(lines.begin(), lines.end());
    _lineStyles.clear();
    _nextStyle = {};

    vector<IInspectable> items;
    items.reserve(lines.size());
    for (auto& line : lines)
    {
      _lineStyles.push_back(_nextStyle);
      if (line.find('\x1B') != string::npos) _nextStyle = NextStyle(_nextStyle, line);
      items.push_back(box_value(to_hstring(line)));
    }
    _lines.ReplaceAll(items);
  }

  void ProcessesPanel::OnLinePrepared(const ItemsRepeater& /*sender*/, const ItemsRepeaterElementPreparedEventArgs& eventArgs)
  {
    auto block = eventArgs.Element().try_as<TextBlock>();
    auto index = size_t(eventArgs.Index());
    if (!block || index >= _rawLines.size()) return;

    block.TextHighlighters().Clear();
    block.Inlines().Clear();

    //Plain lines, most of them, as plain text
    auto& line = _rawLines[index];
    auto& start = _lineStyles[index];
    if (line.find('\x1B') == string::npos && start == ::SecondShell::Text::TextStyle{})
    {
      block.Text(to_hstring(line));
      return;
    }

    int32_t position = 0;
    for (auto& span : ::SecondShell::Text::AnsiParser(start).Parse(line))
    {
      auto text = to_hstring(span.Text);
      auto& style = span.Style;

      Documents::Run run;
      run.Text(text);

      //Inverse swaps the colors; dim mixes the text color halfway into the background
      std::optional<uint32_t> foreground = style.IsInverse ? std::optional<uint32_t>(style.Background.value_or(_backgroundColor)) : style.Foreground;
      std::optional<uint32_t> background = style.IsInverse ? std::optional<uint32_t>(style.Foreground.value_or(_foregroundColor)) : style.Background;
      if (style.IsDim) foreground = Blend(foreground.value_or(_foregroundColor), _backgroundColor);
      if (foreground) run.Foreground(Brush(*foreground));
      if (style.IsBold) run.FontWeight(Microsoft::UI::Text::FontWeights::Bold());
      if (style.IsItalic) run.FontStyle(Windows::UI::Text::FontStyle::Italic);
      if (style.IsUnderlined || style.IsStruck)
      {
        uint32_t decorations = 0;
        if (style.IsUnderlined) decorations |= uint32_t(Windows::UI::Text::TextDecorations::Underline);
        if (style.IsStruck) decorations |= uint32_t(Windows::UI::Text::TextDecorations::Strikethrough);
        run.TextDecorations(Windows::UI::Text::TextDecorations(decorations));
      }
      block.Inlines().Append(run);

      //A run has no background of its own: a highlighter paints it
      if (background)
      {
        Documents::TextHighlighter highlighter;
        highlighter.Background(Brush(*background));
        highlighter.Ranges().Append(Documents::TextRange{ position, int32_t(text.size()) });
        block.TextHighlighters().Append(highlighter);
      }
      position += int32_t(text.size());
    }
  }

  Media::SolidColorBrush ProcessesPanel::Brush(uint32_t color)
  {
    //Not through operator[]: a default brush is a real, transparent one, not an empty handle
    if (auto brush = _brushes.find(color); brush != _brushes.end()) return brush->second;

    Media::SolidColorBrush brush{ Windows::UI::Color{ 255, uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color) } };
    _brushes.emplace(color, brush);
    return brush;
  }

  uint32_t ProcessesPanel::Blend(uint32_t color, uint32_t background)
  {
    uint32_t result = 0;
    for (auto shift : { 16, 8, 0 }) result |= ((((color >> shift) & 0xFF) + ((background >> shift) & 0xFF)) / 2) << shift;
    return result;
  }
  void ProcessesPanel::ShowDetail()
  {
    auto entry = Selected();
    CopyButton().IsEnabled(entry && !_rawLines.empty());

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
        parts > 1 ? std::format("{} files, ", parts) : "", Size(entry->File->Size), _separator, _rawLines.size());
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
    //Without the escape codes, as the agent reads it
    string text;
    for (auto& line : _rawLines) text += ::SecondShell::Text::StripEscapes(line) + "\r\n";

    DataPackage package;
    package.SetText(to_hstring(text));
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
