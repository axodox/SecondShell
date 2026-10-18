#include "pch.h"
#include "ActivityBars.h"
#include "ActionView.h"

using namespace std;
using namespace std::chrono;
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;

namespace SecondShell::Ui
{
  namespace
  {
    //As in the mockups: a bounce of 0.9 s staggered by 0.15 s; when thinking 2.4 s staggered by 0.4 s, lower and dimmer
    struct Motion
    {
      milliseconds Period;
      milliseconds Stagger;
      double Peak;
      double LowOpacity;
      double HighOpacity;
    };

    const Motion _working{ 900ms, 150ms, 1.0, 1.0, 1.0 };
    const Motion _thinking{ 2400ms, 400ms, 0.6, 0.5, 0.8 };
    const double _low = 0.3;
    const double _idleHeight = 5.0 / 14;

    DoubleAnimationUsingKeyFrames Pulse(const UIElement& bar, const wchar_t* property, double low, double high, const Motion& motion, size_t index)
    {
      DoubleAnimationUsingKeyFrames animation;
      SineEase ease;
      ease.EasingMode(EasingMode::EaseInOut);
      for (auto [time, value] : { pair{ 0ms, low }, pair{ motion.Period / 2, high }, pair{ motion.Period, low } })
      {
        EasingDoubleKeyFrame frame;
        frame.KeyTime(KeyTimeHelper::FromTimeSpan(time));
        frame.Value(value);
        frame.EasingFunction(ease);
        animation.KeyFrames().Append(frame);
      }

      animation.BeginTime(winrt::Windows::Foundation::TimeSpan{ motion.Stagger * int(index) });
      animation.RepeatBehavior(RepeatBehaviorHelper::Forever());
      Storyboard::SetTarget(animation, bar);
      Storyboard::SetTargetProperty(animation, property);
      return animation;
    }
  }

  ActivityBars::ActivityBars(const Panel& bars) :
    _bars(bars)
  { }

  StackPanel ActivityBars::MakePanel()
  {
    StackPanel panel;
    panel.Orientation(Orientation::Horizontal);
    panel.Spacing(2);
    panel.Height(14);
    panel.VerticalAlignment(VerticalAlignment::Center);
    for (auto index = 0; index < 4; index++)
    {
      Shapes::Rectangle bar;
      bar.Width(3);
      bar.Height(14);
      bar.RadiusX(1.5);
      bar.RadiusY(1.5);
      bar.RenderTransformOrigin({ 0.5f, 0.5f });
      bar.RenderTransform(ScaleTransform{});
      panel.Children().Append(bar);
    }
    return panel;
  }

  void ActivityBars::Show(Activity activity)
  {
    if (_activity == activity) return;
    _activity = activity;

    if (_storyboard) _storyboard.Stop();
    _storyboard = Storyboard{};

    auto brush = ActionView::Resource<Brush>(activity == Activity::Idle ? L"IdleBarBrush" : L"AgentBrush");
    auto& motion = activity == Activity::Working ? _working : _thinking;
    size_t index = 0;
    for (auto&& child : _bars.Children())
    {
      auto bar = child.as<Shapes::Rectangle>();
      bar.Fill(brush);
      bar.Opacity(1);
      bar.RenderTransform().as<ScaleTransform>().ScaleY(activity == Activity::Idle ? _idleHeight : _low);
      if (activity == Activity::Idle) continue;

      _storyboard.Children().Append(Pulse(bar, L"(UIElement.RenderTransform).(ScaleTransform.ScaleY)", _low, motion.Peak, motion, index));
      if (motion.LowOpacity < 1) _storyboard.Children().Append(Pulse(bar, L"Opacity", motion.LowOpacity, motion.HighOpacity, motion, index));
      index++;
    }

    if (activity != Activity::Idle) _storyboard.Begin();
  }
}
