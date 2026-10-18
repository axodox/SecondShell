#include "pch.h"
#include "SessionDialogs.h"
#include "ActionView.h"
#include "ActivityBars.h"
#include "SequenceView.h"

using namespace SecondShell::Agent;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;
using namespace winrt;
using namespace winrt::Microsoft::UI::Text;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;

namespace SecondShell::Ui
{
  //In UTF-8, whatever the compiler's execution character set
  const char* const _separator = " \xC2\xB7 ";

  namespace
  {
    TextBlock Paragraph(string_view text, wstring_view brush = {})
    {
      auto block = ActionView::Text(text, brush, 14);
      block.TextWrapping(TextWrapping::Wrap);
      return block;
    }

    //What the question is about, in a box: a marker, a name, and details
    Border Subject(const UIElement& marker, string_view name, string_view details)
    {
      StackPanel row;
      row.Orientation(Orientation::Horizontal);
      row.Spacing(9);
      row.Children().Append(marker);

      auto title = ActionView::Text(name, {}, 13);
      title.FontWeight(FontWeights::SemiBold());
      row.Children().Append(title);
      row.Children().Append(ActionView::Text(details, L"MutedTextBrush", 13));

      Border box;
      box.Padding({ 12, 10, 12, 10 });
      box.CornerRadius({ 8, 8, 8, 8 });
      box.BorderThickness({ 1, 1, 1, 1 });
      box.Background(ActionView::Resource<Brush>(L"GroupBrush"));
      box.BorderBrush(ActionView::Resource<Brush>(L"GroupBorderBrush"));
      box.Child(row);
      return box;
    }

    ContentDialog Dialog(const XamlRoot& root, const wchar_t* title, const StackPanel& content, const wchar_t* primary, const wchar_t* close)
    {
      ContentDialog dialog;
      dialog.XamlRoot(root);
      dialog.Style(ActionView::Resource<Style>(L"DefaultContentDialogStyle"));
      dialog.Title(box_value(title));
      dialog.Content(content);
      dialog.PrimaryButtonText(primary);
      dialog.CloseButtonText(close);

      //Both questions end apps in the session: the safe answer is the default
      dialog.DefaultButton(ContentDialogButton::Close);
      return dialog;
    }

    //A line of a checklist: a tick or a cross, and what it is about
    StackPanel Check(bool isDone, string_view text)
    {
      StackPanel row;
      row.Orientation(Orientation::Horizontal);
      row.Spacing(9);

      FontIcon icon;
      icon.Glyph(isDone ? L"\uE73E" : L"\uE711");
      icon.FontSize(12);
      icon.Foreground(ActionView::Resource<Brush>(isDone ? L"SuccessBrush" : L"FailureBrush"));
      row.Children().Append(icon);
      row.Children().Append(ActionView::Text(text, {}, 13));
      return row;
    }

    StackPanel Content()
    {
      StackPanel content;
      content.Spacing(12);
      content.MaxWidth(440);
      return content;
    }
  }

  IAsyncOperation<bool> SessionDialogs::AskToReset(XamlRoot root, ExistingSession session)
  {
    auto content = Content();
    content.Children().Append(Paragraph("A child session from an earlier run is still signed in, with its apps open. SecondShell cannot resume it yet: reset it to start clean, or cancel to keep it."));

    Shapes::Ellipse marker;
    marker.Width(8);
    marker.Height(8);
    marker.Fill(ActionView::Resource<Brush>(L"AgentBrush"));
    marker.VerticalAlignment(VerticalAlignment::Center);

    auto details = session.LogonTime ? format("signed in at {:%H:%M}{}disconnected", zoned_time{ current_zone(), floor<minutes>(*session.LogonTime) }, _separator) : string("disconnected");
    content.Children().Append(Subject(marker, format("Session {}", session.SessionId), details));
    content.Children().Append(Paragraph("Resetting signs it out; apps in it close without saving.", L"MutedTextBrush"));

    auto dialog = Dialog(root, L"A session is still running", content, L"Reset", L"Cancel");
    co_return co_await dialog.ShowAsync() == ContentDialogResult::Primary;
  }

  IAsyncOperation<bool> SessionDialogs::AskToCloseWhileWorking(XamlRoot root, SequenceLogEntry sequence)
  {
    auto content = Content();
    content.Children().Append(Paragraph("Closing SecondShell stops its sequence and signs the session out. Apps in the session close without saving."));

    //The bars keep moving while the dialog is open
    auto bars = ActivityBars::MakePanel();
    ActivityBars activity{ bars };
    activity.Show(Activity::Working);
    content.Children().Append(Subject(bars, SequenceView::Title(sequence), format("step {} of {}", max<size_t>(sequence.Steps.size(), 1), sequence.StepCount)));

    auto dialog = Dialog(root, L"The agent is working", content, L"Close and sign out", L"Keep open");
    co_return co_await dialog.ShowAsync() == ContentDialogResult::Primary;
  }

  IAsyncOperation<IReference<bool>> SessionDialogs::AskToSetUp(XamlRoot root, Setup::SetupStatus status)
  {
    auto content = Content();
    content.Children().Append(Paragraph("SecondShell runs the agent in a child session, a second desktop of your own account. Child sessions are not enabled on this PC yet."));

    StackPanel checks;
    checks.Spacing(6);
    checks.Children().Append(Check(status.AreChildSessionsEnabled, "Child sessions enabled"));
    content.Children().Append(checks);

    content.Children().Append(Paragraph("Enabling them needs administrator rights once; SecondShell itself keeps running without them.", L"MutedTextBrush"));

    //Offered on, unless it is on already (Design.md, Smooth session view)
    CheckBox smoothView{ nullptr };
    if (!status.IsSmoothSessionViewOn)
    {
      smoothView = CheckBox();
      smoothView.IsChecked(true);
      smoothView.Content(box_value(L"Smooth session view (60 fps)"));
      content.Children().Append(smoothView);
      content.Children().Append(Paragraph("Every Remote Desktop session on this PC is then drawn at 60 fps instead of 30, from its next sign-in; incoming connections too.", L"MutedTextBrush"));
    }

    auto dialog = Dialog(root, L"One-time setup needed", content, L"Set up", L"Not now");
    dialog.DefaultButton(ContentDialogButton::Primary);
    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) co_return nullptr;

    co_return box_value(smoothView && smoothView.IsChecked().Value()).as<IReference<bool>>();
  }
}
