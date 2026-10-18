#include "pch.h"
#include "ActionView.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace winrt;
using namespace winrt::Microsoft::UI::Text;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace SecondShell::Ui
{
  namespace
  {
    TextBlock ChipText(string_view text, wstring_view brush)
    {
      TextBlock block;
      block.Style(ActionView::Resource<Style>(L"ChipTextStyle"));
      block.Text(to_hstring(text));
      if (!brush.empty()) block.Foreground(ActionView::Resource<Brush>(brush));
      return block;
    }

    Border Keycap(string_view key)
    {
      Border border;
      border.Style(ActionView::Resource<Style>(L"KeycapStyle"));
      border.Child(ChipText(key, {}));
      return border;
    }

    Border Chip(const ActionArgument& argument)
    {
      StackPanel content;
      content.Orientation(Orientation::Horizontal);
      content.Spacing(5);
      content.Children().Append(ChipText(argument.Name, L"MutedTextBrush"));
      content.Children().Append(ChipText(argument.Value, argument.IsText ? L"StringTextBrush" : L""));

      Border border;
      border.Style(ActionView::Resource<Style>(L"ChipStyle"));
      border.Child(content);
      return border;
    }

    FontIcon Glyph(wchar_t glyph, wstring_view brush)
    {
      FontIcon icon;
      icon.Glyph(hstring(wstring(1, glyph)));
      icon.FontSize(11);
      icon.Foreground(ActionView::Resource<Brush>(brush));
      icon.VerticalAlignment(VerticalAlignment::Center);
      return icon;
    }
  }

  StackPanel ActionView::Build(const ActionLogEntry& entry)
  {
    StackPanel panel;
    panel.Orientation(Orientation::Horizontal);
    panel.Spacing(6);
    panel.VerticalAlignment(VerticalAlignment::Center);

    auto& presentation = entry.Presentation;
    panel.Children().Append(StatusIcon(entry.Status));

    auto name = Text(presentation.Name, L"AgentTextBrush");
    name.FontWeight(FontWeights::SemiBold());
    panel.Children().Append(name);

    for (auto& key : presentation.Keys)
    {
      if (&key != &presentation.Keys.front()) panel.Children().Append(Text("+", L"FaintTextBrush", 11));
      panel.Children().Append(Keycap(key));
    }

    for (auto& argument : presentation.Arguments) panel.Children().Append(Chip(argument));
    return panel;
  }

  UIElement ActionView::StatusIcon(const std::optional<ActionStatus>& status)
  {
    if (!status)
    {
      ProgressRing ring;
      ring.IsActive(true);
      ring.Width(12);
      ring.Height(12);
      ring.MinWidth(12);
      ring.MinHeight(12);
      ring.Foreground(Resource<Brush>(L"AgentTextBrush"));
      ring.VerticalAlignment(VerticalAlignment::Center);
      return ring;
    }

    switch (*status)
    {
    case ActionStatus::Succeeded:
      return Glyph(0xE73E, L"SuccessBrush");
    case ActionStatus::Failed:
      return Glyph(0xE711, L"FailureBrush");
    default:
      return Glyph(0xE71A, L"MutedTextBrush");
    }
  }

  TextBlock ActionView::Text(string_view text, wstring_view brush, double fontSize)
  {
    TextBlock block;
    block.Text(to_hstring(text));
    block.FontSize(fontSize);
    block.VerticalAlignment(VerticalAlignment::Center);
    if (!brush.empty()) block.Foreground(Resource<Brush>(brush));
    return block;
  }
}
