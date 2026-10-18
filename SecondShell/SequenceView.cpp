#include "pch.h"
#include "SequenceView.h"
#include "ActionView.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;
using namespace winrt;
using namespace winrt::Microsoft::UI::Text;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation;

namespace SecondShell::Ui
{
  //In UTF-8, whatever the compiler's execution character set
  const char* const _separator = " \xC2\xB7 ";
  const char* const _times = " \xC3\x97 ";

  //The details of a step line up after its time column
  const double _detailIndent = 52;
  const size_t _titleVerbs = 3;

  namespace
  {
    string Offset(milliseconds offset)
    {
      if (offset < 100ms) return format("{} ms", offset.count());
      auto seconds = offset.count() / 1000.0;
      return seconds < 10 ? format("{:.1f} s", seconds) : format("{:.0f} s", seconds);
    }

    string Clock(system_clock::time_point time)
    {
      return format("{:%H:%M:%S}", zoned_time{ current_zone(), floor<seconds>(time) });
    }

    string Size(uint64_t bytes)
    {
      return bytes < 1024 * 1024 ? format("{} KB", (bytes + 1023) / 1024) : format("{:.1f} MB", bytes / (1024.0 * 1024.0));
    }

    milliseconds Elapsed(system_clock::time_point since)
    {
      return duration_cast<milliseconds>(system_clock::now() - since);
    }

    //A step's verb for the title made from steps: "click" for Mouse.Click
    string Verb(const string& action)
    {
      auto kind = action.substr(0, action.find('.'));
      if (kind == "Capture") return "capture";
      if (kind == "Clipboard") return "clipboard";

      auto verb = action.substr(action.find('.') + 1);
      ranges::transform(verb, verb.begin(), [](char character) { return char(tolower(character)); });
      return verb;
    }

    void CopyText(const hstring& text)
    {
      DataPackage package;
      package.SetText(text);
      Clipboard::SetContent(package);
    }

    Button SmallButton(const wchar_t* label, function<void()> action)
    {
      Button button;
      button.Content(box_value(label));
      button.Style(ActionView::Resource<Style>(L"SmallButtonStyle"));
      button.Click([action](auto&&, auto&&) { action(); });
      return button;
    }

    Border Badge(const SequenceLogEntry& sequence)
    {
      auto isRunning = !sequence.Duration;
      auto isFailed = sequence.Error.has_value();
      auto label = isRunning ? "Running" : !isFailed ? "Done" : sequence.Error == ErrorCode::UserInterrupted ? "Interrupted" : "Failed";

      auto text = ActionView::Text(label, isRunning ? L"RunningBadgeTextBrush" : isFailed ? L"FailureBrush" : L"SessionTextBrush", 11);
      Border badge;
      badge.Padding({ 7, 0, 7, 1 });
      badge.CornerRadius({ 9, 9, 9, 9 });
      badge.Background(ActionView::Resource<Brush>(isRunning ? L"RunningBadgeBrush" : isFailed ? L"FailedBadgeBrush" : L"BadgeBrush"));
      badge.VerticalAlignment(VerticalAlignment::Center);
      badge.Child(text);
      return badge;
    }

    Grid Header(const SequenceLogEntry& sequence)
    {
      Grid header;
      header.Padding({ 10, 8, 10, 8 });
      header.ColumnSpacing(8);
      for (auto width : { GridLengthHelper::Auto(), GridLengthHelper::Auto(), GridLengthHelper::Auto(), GridLengthHelper::FromValueAndType(1, GridUnitType::Star), GridLengthHelper::Auto() })
      {
        ColumnDefinition column;
        column.Width(width);
        header.ColumnDefinitions().Append(column);
      }

      auto title = ActionView::Text(SequenceView::Title(sequence));
      title.FontWeight(FontWeights::SemiBold());
      title.TextTrimming(TextTrimming::CharacterEllipsis);

      auto duration = sequence.Duration.value_or(Elapsed(sequence.Time));
      auto details = ActionView::Text(format("#{}{}{} of {} steps{}{}", sequence.Number, _separator, sequence.Steps.size(), sequence.StepCount, _separator, FormatDuration(duration)), L"MutedTextBrush");

      auto clock = ActionView::Text(Clock(sequence.Time), L"MutedTextBrush", 11);
      clock.FontFamily(ActionView::Resource<FontFamily>(L"MonoFontFamily"));

      UIElement cells[] = { title, details, Badge(sequence), clock };
      int columns[] = { 0, 1, 2, 4 };
      for (size_t index = 0; index < size(cells); index++)
      {
        Grid::SetColumn(cells[index].as<FrameworkElement>(), columns[index]);
        header.Children().Append(cells[index]);
      }
      return header;
    }

    UIElement CaptureDetails(const CaptureLogInfo& capture)
    {
      Grid details;
      details.ColumnSpacing(12);
      details.Margin({ _detailIndent, 0, 0, 0 });
      ColumnDefinition imageColumn, infoColumn;
      imageColumn.Width(GridLengthHelper::Auto());
      infoColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      details.ColumnDefinitions().Append(imageColumn);
      details.ColumnDefinitions().Append(infoColumn);

      Border frame;
      frame.Width(176);
      frame.Height(99);
      frame.CornerRadius({ 6, 6, 6, 6 });
      frame.BorderThickness({ 1, 1, 1, 1 });
      frame.BorderBrush(ActionView::Resource<Brush>(L"ChipBorderBrush"));
      frame.Background(ActionView::Resource<Brush>(L"OutputBrush"));
      if (!capture.File.empty() && filesystem::exists(to_hstring(capture.File).c_str()))
      {
        BitmapImage bitmap;
        bitmap.DecodePixelWidth(352);
        auto uri = wstring(to_hstring(capture.File));
        ranges::replace(uri, L'\\', L'/');
        bitmap.UriSource(Uri{ L"file:///" + uri });

        Image image;
        image.Source(bitmap);
        image.Stretch(Stretch::UniformToFill);
        frame.Child(image);
      }
      details.Children().Append(frame);

      StackPanel info;
      info.Spacing(6);
      Grid::SetColumn(info, 1);

      auto bytes = capture.Bytes;
      if (!bytes && !capture.File.empty())
      {
        error_code error;
        bytes = filesystem::file_size(to_hstring(capture.File).c_str(), error);
      }
      info.Children().Append(ActionView::Text(format("{}{}{}{}{}{}{}{}", capture.Format == ImageFormat::Jpeg ? "JPEG" : "PNG", _separator, capture.Width, _times, capture.Height, _separator, Size(bytes), capture.IsInline ? string(_separator) + "sent inline" : string(_separator) + "saved to file")));

      if (!capture.File.empty())
      {
        auto path = ActionView::Text(capture.File, L"MutedTextBrush", 11);
        path.FontFamily(ActionView::Resource<FontFamily>(L"MonoFontFamily"));
        path.TextTrimming(TextTrimming::CharacterEllipsis);
        info.Children().Append(path);

        StackPanel buttons;
        buttons.Orientation(Orientation::Horizontal);
        buttons.Spacing(6);
        auto file = to_hstring(capture.File);
        buttons.Children().Append(SmallButton(L"Open", [file] { ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }));
        buttons.Children().Append(SmallButton(L"Copy path", [file] { CopyText(file); }));
        info.Children().Append(buttons);
      }
      details.Children().Append(info);
      return details;
    }

    UIElement OutputDetails(const ProcessLogInfo& process)
    {
      Grid header;
      header.Padding({ 10, 6, 6, 6 });
      ColumnDefinition textColumn, buttonColumn;
      textColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      buttonColumn.Width(GridLengthHelper::Auto());
      header.ColumnDefinitions().Append(textColumn);
      header.ColumnDefinitions().Append(buttonColumn);

      auto exit = process.ExitCode ? format("Output{}exit code {}", _separator, *process.ExitCode) : format("Output{}PID {}", _separator, process.ProcessId);
      header.Children().Append(ActionView::Text(exit, L"MutedTextBrush", 11));
      auto copy = SmallButton(L"Copy", [output = to_hstring(process.Output)] { CopyText(output); });
      Grid::SetColumn(copy, 1);
      header.Children().Append(copy);

      auto text = ActionView::Text(process.Output, L"", 11);
      text.FontFamily(ActionView::Resource<FontFamily>(L"MonoFontFamily"));
      text.TextWrapping(TextWrapping::Wrap);
      text.IsTextSelectionEnabled(true);
      text.Margin({ 10, 0, 10, 8 });

      ScrollViewer scroller;
      scroller.MaxHeight(160);
      scroller.Content(text);

      StackPanel content;
      content.Children().Append(header);
      content.Children().Append(scroller);

      Border box;
      box.Margin({ _detailIndent, 0, 0, 0 });
      box.CornerRadius({ 6, 6, 6, 6 });
      box.BorderThickness({ 1, 1, 1, 1 });
      box.BorderBrush(ActionView::Resource<Brush>(L"FieldBorderBrush"));
      box.Background(ActionView::Resource<Brush>(L"OutputBrush"));
      box.Child(content);
      return box;
    }

    UIElement Step(const SequenceLogEntry& sequence, const ActionLogEntry& step)
    {
      Grid line;
      line.ColumnSpacing(8);
      for (auto width : { GridLengthHelper::FromPixels(44), GridLengthHelper::Auto(), GridLengthHelper::FromValueAndType(1, GridUnitType::Star), GridLengthHelper::Auto() })
      {
        ColumnDefinition column;
        column.Width(width);
        line.ColumnDefinitions().Append(column);
      }

      auto offset = ActionView::Text(Offset(duration_cast<milliseconds>(step.Time - sequence.Time)), L"FaintTextBrush", 11);
      offset.FontFamily(ActionView::Resource<FontFamily>(L"MonoFontFamily"));
      line.Children().Append(offset);

      auto action = ActionView::Build(step);
      Grid::SetColumn(action, 1);
      line.Children().Append(action);

      auto isRunning = !step.Status;
      auto duration = ActionView::Text(FormatDuration(step.Duration.value_or(Elapsed(step.Time))), isRunning ? L"RunningBadgeTextBrush" : L"FaintTextBrush", 11);
      Grid::SetColumn(duration, 3);
      line.Children().Append(duration);

      StackPanel panel;
      panel.Spacing(8);
      panel.Padding({ 6, 5, 6, 5 });
      panel.CornerRadius({ 6, 6, 6, 6 });
      if (isRunning) panel.Background(ActionView::Resource<Brush>(L"RunningStepBrush"));
      panel.Children().Append(line);

      if (step.Status == ActionStatus::Failed && !step.Message.empty())
      {
        auto reason = ActionView::Text(step.Message, L"FailureBrush");
        reason.TextWrapping(TextWrapping::Wrap);
        reason.Margin({ _detailIndent, 0, 0, 0 });
        panel.Children().Append(reason);
      }
      if (step.Capture) panel.Children().Append(CaptureDetails(*step.Capture));
      if (step.Process && !step.Process->Output.empty()) panel.Children().Append(OutputDetails(*step.Process));
      return panel;
    }
  }

  Border SequenceView::Build(const SequenceLogEntry& sequence, const StepFilter& filter)
  {
    StackPanel steps;
    steps.Padding({ 6, 0, 6, 6 });
    for (auto& step : sequence.Steps)
    {
      if (filter(step)) steps.Children().Append(Step(sequence, step));
    }
    if (steps.Children().Size() == 0) return nullptr;

    StackPanel content;
    content.Children().Append(Header(sequence));
    content.Children().Append(steps);

    Border group;
    group.CornerRadius({ 8, 8, 8, 8 });
    group.BorderThickness({ 1, 1, 1, 1 });
    group.Background(ActionView::Resource<Brush>(L"GroupBrush"));
    group.BorderBrush(ActionView::Resource<Brush>(L"GroupBorderBrush"));
    group.Child(content);
    return group;
  }

  string SequenceView::Title(const SequenceLogEntry& sequence)
  {
    if (!sequence.Description.empty()) return sequence.Description;

    vector<string> verbs;
    for (auto& step : sequence.Steps | views::take(_titleVerbs))
    {
      auto verb = Verb(step.Presentation.Name);
      if (verbs.empty() || verbs.back() != verb) verbs.push_back(verb);
    }
    if (verbs.empty()) return "Agent working";

    verbs.front()[0] = char(toupper(verbs.front()[0]));
    auto more = sequence.StepCount > _titleVerbs ? sequence.StepCount - uint32_t(_titleVerbs) : 0;

    string title;
    for (size_t index = 0; index < verbs.size(); index++)
    {
      auto isLast = index + 1 == verbs.size();
      title += index == 0 ? "" : isLast && !more ? " and " : ", ";
      title += verbs[index];
    }
    if (more) title += format(" and {} more", more);
    return title;
  }
}
