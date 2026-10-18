#pragma once

namespace SecondShell::Ui
{
  enum class Activity
  {
    Working,   //a sequence runs: the bars bounce
    Thinking,  //between sequences: slower and dimmer
    Idle       //nothing for a while: short and grey
  };

  //The activity indicator of the status bar: bars drawn in XAML, animated from here.
  class ActivityBars
  {
  public:
    ActivityBars() = default;
    explicit ActivityBars(const winrt::Microsoft::UI::Xaml::Controls::Panel& bars);

    //The bars, as the status bar's XAML draws them, for views built in code.
    static winrt::Microsoft::UI::Xaml::Controls::StackPanel MakePanel();

    void Show(Activity activity);

  private:
    winrt::Microsoft::UI::Xaml::Controls::Panel _bars{ nullptr };
    winrt::Microsoft::UI::Xaml::Media::Animation::Storyboard _storyboard{ nullptr };
    std::optional<Activity> _activity;
  };
}
