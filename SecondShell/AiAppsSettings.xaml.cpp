#include "pch.h"
#include "AiAppsSettings.xaml.h"
#if __has_include("AiAppsSettings.g.cpp")
#include "AiAppsSettings.g.cpp"
#endif
#include "ActionView.h"
#include "SessionDialogs.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>

using namespace ::SecondShell::Integrations;
using namespace ::SecondShell::Ui;
using namespace Axodox::Infrastructure;
using namespace std;
using namespace winrt::Microsoft::UI::Text;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Automation;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  namespace
  {
    //"CC" for Claude Code, "WD" for Windsurf (Devin Desktop), "Cu" for Cursor
    string Initials(string_view name)
    {
      string initials;
      for (auto word : views::split(name, ' '))
      {
        auto letter = ranges::find_if(word, [](char character) { return isalnum(uint8_t(character)) != 0; });
        if (letter != word.end()) initials += *letter;
        if (initials.size() == 2) return initials;
      }
      return string{ name.substr(0, 2) };
    }

    struct StateLook
    {
      const char* Text;
      const wchar_t* Brush;
    };

    StateLook LookOf(IntegrationState state)
    {
      switch (state)
      {
      case IntegrationState::Added: return { "Added", L"SuccessBrush" };
      case IntegrationState::Outdated: return { "Out of date", L"UserTextBrush" };
      case IntegrationState::NotAdded: return { "Not added", L"SecondaryTextBrush" };
      case IntegrationState::Unreadable: return { "Cannot read", L"FailureBrush" };
      default: return { "", L"MutedTextBrush" };
      }
    }

    string DetailOf(const AiClientIntegration& integration, IntegrationState state)
    {
      switch (state)
      {
      case IntegrationState::NotFound: return "Not found on this PC";
      case IntegrationState::Outdated: return "Its entry runs SecondShell from another folder";
      case IntegrationState::Unreadable: return "Its configuration file is not valid JSON, or cannot be read; it is left alone";
      default: return integration.Where();
      }
    }
  }

  AiAppsSettings::AiAppsSettings()
  {
    InitializeComponent();
  }

  void AiAppsSettings::UseServices(dependency_container* container)
  {
    _integrations = container->resolve<IntegrationManager>();
    SnippetText().Text(to_hstring(_integrations->Snippet()));
    Refresh();
  }

  void AiAppsSettings::Refresh()
  {
    //The first page is selected while the window is made, before the services are there
    if (!_integrations) return;

    AppList().Children().Clear();
    for (auto& integration : _integrations->Integrations())
    {
      AppList().Children().Append(Card(integration, integration->State()));
    }
  }

  void AiAppsSettings::OnCopySnippetClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    DataPackage package;
    package.SetText(SnippetText().Text());
    Clipboard::SetContent(package);
  }

  Grid AiAppsSettings::Card(const shared_ptr<AiClientIntegration>& integration, IntegrationState state)
  {
    Grid card;
    card.Style(ActionView::Resource<Microsoft::UI::Xaml::Style>(L"SettingsCardStyle"));
    for (auto width : { GridLength{ 0, GridUnitType::Auto }, GridLength{ 1, GridUnitType::Star }, GridLength{ 0, GridUnitType::Auto }, GridLength{ 0, GridUnitType::Auto } })
    {
      ColumnDefinition column;
      column.Width(width);
      card.ColumnDefinitions().Append(column);
    }

    //The app's mark: its initials in a square
    Border badge;
    badge.Width(32);
    badge.Height(32);
    badge.CornerRadius({ 6, 6, 6, 6 });
    badge.Background(ActionView::Resource<Brush>(L"ChipBrush"));
    badge.BorderBrush(ActionView::Resource<Brush>(L"ChipBorderBrush"));
    badge.BorderThickness({ 1, 1, 1, 1 });
    badge.VerticalAlignment(VerticalAlignment::Center);
    auto initials = ActionView::Text(Initials(integration->Name()), L"SecondaryTextBrush", 12);
    initials.FontWeight(FontWeights::SemiBold());
    initials.HorizontalAlignment(HorizontalAlignment::Center);
    initials.VerticalAlignment(VerticalAlignment::Center);
    badge.Child(initials);
    card.Children().Append(badge);

    StackPanel text;
    text.Spacing(2);
    text.VerticalAlignment(VerticalAlignment::Center);
    auto name = ActionView::Text(integration->Name(), {}, 13);
    name.Style(ActionView::Resource<Microsoft::UI::Xaml::Style>(L"SettingsNameStyle"));
    text.Children().Append(name);

    auto problem = _problems.find(string{ integration->Id() });
    auto detail = ActionView::Text(problem != _problems.end() ? problem->second : DetailOf(*integration, state), problem != _problems.end() ? L"FailureBrush" : L"SecondaryTextBrush", 12);
    detail.TextWrapping(TextWrapping::Wrap);
    text.Children().Append(detail);
    Grid::SetColumn(text, 1);
    card.Children().Append(text);

    auto look = LookOf(state);
    auto stateText = ActionView::Text(look.Text, look.Brush, 12);
    stateText.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(stateText, 2);
    card.Children().Append(stateText);

    //What can be done: add, update an entry that runs another SecondShell, or remove it
    if (state == IntegrationState::NotAdded || state == IntegrationState::Outdated || state == IntegrationState::Added)
    {
      Button button;
      auto action = state == IntegrationState::Added ? L"Remove" : state == IntegrationState::Outdated ? L"Update" : L"Add";
      button.Content(box_value(action));
      AutomationProperties::SetName(button, hstring{ action } + (state == IntegrationState::Added ? L" SecondShell from " : L" SecondShell to ") + to_hstring(integration->Name()));
      if (state != IntegrationState::Added) button.Style(ActionView::Resource<Microsoft::UI::Xaml::Style>(L"AccentButtonStyle"));
      button.MinWidth(84);
      button.VerticalAlignment(VerticalAlignment::Center);
      button.IsEnabled(!_isChanging);
      button.Click([weak = get_weak(), integration, isAdding = state != IntegrationState::Added](auto&&, auto&&) {
        if (auto self = weak.get()) self->Change(integration, isAdding);
        });
      Grid::SetColumn(button, 3);
      card.Children().Append(button);
    }
    return card;
  }

  fire_and_forget AiAppsSettings::Change(shared_ptr<AiClientIntegration> integration, bool isAdding)
  {
    auto self = get_strong();
    if (_isChanging) co_return;

    auto id = string{ integration->Id() };
    _problems.erase(id);

    auto name = to_hstring(integration->Name());
    auto isUpdate = isAdding && integration->State() == IntegrationState::Outdated;
    IntegrationChange change;
    try
    {
      change = isAdding ? integration->PreviewAdd() : integration->PreviewRemove();
    }
    catch (const exception& exception)
    {
      _problems[id] = exception.what();
      Refresh();
      co_return;
    }

    auto title = isUpdate ? L"Update SecondShell in " + name + L"?" : isAdding ? L"Add SecondShell to " + name + L"?" : L"Remove SecondShell from " + name + L"?";
    if (!co_await SessionDialogs::AskToChangeIntegration(XamlRoot(), title, isUpdate ? L"Update" : isAdding ? L"Add" : L"Remove", change)) co_return;

    //An app's command line can take a while: not on the UI thread
    _isChanging = true;
    Refresh();
    auto queue = DispatcherQueue();
    string problem;
    co_await resume_background();
    try
    {
      if (isAdding) integration->Add();
      else integration->Remove();
    }
    catch (const winrt::hresult_error& error)
    {
      problem = to_string(error.message());
    }
    catch (const exception& exception)
    {
      problem = exception.what();
    }
    co_await wil::resume_foreground(queue);

    _isChanging = false;
    if (!problem.empty()) _problems[id] = problem;
    Refresh();
  }
}
