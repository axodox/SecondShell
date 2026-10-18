#pragma once
#include "ActionLog.h"

namespace SecondShell::Ui
{
  //An action as the status bar and the log draw it (docs/UIDesign.md, Status bar): its result, its name in the agent
  //colour, the keys it presses as keycaps and its arguments as chips, styled by the theme.
  class ActionView
  {
  public:
    static winrt::Microsoft::UI::Xaml::Controls::StackPanel Build(const Agent::ActionLogEntry& entry);

    //A spinner while it runs, then a check, a cross, or a square when it was cancelled.
    static winrt::Microsoft::UI::Xaml::UIElement StatusIcon(const std::optional<Actions::ActionStatus>& status);

    //A theme resource by key.
    template<typename T>
    static T Resource(std::wstring_view key)
    {
      return winrt::Microsoft::UI::Xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<T>();
    }

    static winrt::Microsoft::UI::Xaml::Controls::TextBlock Text(std::string_view text, std::wstring_view brush = {}, double fontSize = 12);
  };
}
