#include "pch.h"
#include "ActivityCards.h"
#include "ActionView.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Agent;
using namespace SecondShell::Rendering;
using namespace std;
using namespace std::chrono;
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;

namespace SecondShell::Ui
{
  const size_t _maxCards = 8;
  const milliseconds _tick{ 200 };
  const milliseconds _shownDuration{ 4000 };
  const milliseconds _fadeDuration{ 500 };
  const milliseconds _slideDuration{ 200 };
  const double _slideDistance = 12;
  const double _margin = 12;  //from the session's corner, in device independent pixels

  namespace
  {
    //The actions the cursor does not show already
    bool IsShown(const string& action)
    {
      return action.starts_with("Keyboard.") || action.starts_with("Clipboard.") || action == "Process.Run" || action == "Process.Stop" || action == "Process.Kill";
    }

    bool IsRelease(const string& action)
    {
      return action == "Keyboard.Release" || action == "Keyboard.Reset" || action == "Control.Reset";
    }

    Storyboard Animate(const UIElement& target, const wchar_t* property, double from, double to, milliseconds duration)
    {
      DoubleAnimation animation;
      animation.From(from);
      animation.To(to);
      animation.Duration(DurationHelper::FromTimeSpan(duration));
      QuadraticEase ease;
      ease.EasingMode(EasingMode::EaseOut);
      animation.EasingFunction(ease);
      Storyboard::SetTarget(animation, target);
      Storyboard::SetTargetProperty(animation, property);

      Storyboard storyboard;
      storyboard.Children().Append(animation);
      return storyboard;
    }
  }

  ActivityCards::ActivityCards(dependency_container* container, const StackPanel& panel) :
    _actionLog(container->resolve<ActionLog>()),
    _renderer(container->resolve<SessionRenderer>()),
    _panel(panel),
    _queue(panel.DispatcherQueue())
  {
    _subscriptions.push_back(_actionLog->ActionStarted([this](ActionLog*, const ActionLogEntry& step) { _queue.TryEnqueue([this, step] { OnStarted(step); }); }));
    _subscriptions.push_back(_actionLog->EntryAdded([this](ActionLog*, const ActionLogEntry& step) { _queue.TryEnqueue([this, step] { OnFinished(step); }); }));

    _timer = _queue.CreateTimer();
    _timer.Interval(_tick);
    _timer.Tick([this](auto&&, auto&&) { Tick(); });
    _timer.Start();
  }

  void ActivityCards::OnStarted(const ActionLogEntry& step)
  {
    auto& action = step.Presentation.Name;
    if (IsRelease(action))
    {
      Release(action == "Keyboard.Release" && !step.Presentation.Keys.empty() ? std::optional{ step.Presentation.Keys.back() } : std::nullopt);
      if (action == "Keyboard.Release") return;
    }

    if (IsShown(action)) Add(step);
  }

  void ActivityCards::OnFinished(const ActionLogEntry& step)
  {
    auto card = ranges::find_if(_cards, [&](const Card& card) { return card.Sequence == step.Sequence && card.Step == step.Step && !card.IsLeaving; });
    if (card != _cards.end()) Show(*card, step);
  }

  void ActivityCards::Add(const ActionLogEntry& step)
  {
    if (_cards.size() == _maxCards)
    {
      uint32_t index;
      if (_panel.Children().IndexOf(_cards.front().View, index)) _panel.Children().RemoveAt(index);
      _cards.pop_front();
    }

    Card card{ step.Sequence, step.Step };
    if (step.Presentation.Name == "Keyboard.Press" && !step.Presentation.Keys.empty()) card.HeldKey = step.Presentation.Keys.back();

    card.View = Border{};
    card.View.Style(ActionView::Resource<Style>(L"ActivityCardStyle"));
    card.View.Opacity(0);
    card.View.RenderTransform(TranslateTransform{});
    Show(card, step);
    _panel.Children().Append(card.View);

    //It slides in from below; the others move up with the panel's reposition transition
    Animate(card.View, L"Opacity", 0, 1, _slideDuration).Begin();
    Animate(card.View, L"(UIElement.RenderTransform).(TranslateTransform.Y)", _slideDistance, 0, _slideDuration).Begin();
    _cards.push_back(move(card));
    Anchor();
  }

  void ActivityCards::Release(const std::optional<string>& key)
  {
    for (auto& card : _cards)
    {
      if (!card.HeldKey || (key && card.HeldKey != key)) continue;

      card.HeldKey.reset();
      card.Shown = steady_clock::now();
      card.View.Background(ActionView::Resource<Brush>(L"ActivityCardBrush"));
    }
  }

  void ActivityCards::Show(Card& card, const ActionLogEntry& step)
  {
    card.Shown = steady_clock::now();
    card.View.Child(ActionView::Build(step));
    card.View.Background(ActionView::Resource<Brush>(card.HeldKey ? L"HeldActivityCardBrush" : L"ActivityCardBrush"));
  }

  void ActivityCards::Leave(Card& card)
  {
    card.IsLeaving = true;
    auto fade = Animate(card.View, L"Opacity", 1, 0, _fadeDuration);
    fade.Completed([this, view = card.View](auto&&, auto&&) {
      uint32_t index;
      if (_panel.Children().IndexOf(view, index)) _panel.Children().RemoveAt(index);
      erase_if(_cards, [&](const Card& card) { return card.View == view; });
      });
    fade.Begin();
  }

  void ActivityCards::Tick()
  {
    Anchor();

    auto now = steady_clock::now();
    for (auto& card : _cards)
    {
      if (!card.HeldKey && !card.IsLeaving && now - card.Shown >= _shownDuration) Leave(card);
    }
  }

  void ActivityCards::Anchor()
  {
    //The session is letterboxed in the view, which can change with every frame size or window size
    auto transform = _renderer->Transform();
    auto view = _panel.Parent().try_as<FrameworkElement>();
    if (!transform || !view) return;

    auto left = transform->Session.left / transform->UiScale + _margin;
    auto bottom = view.ActualHeight() - transform->Session.bottom / transform->UiScale + _margin;
    _panel.Margin({ left, 0, 0, max(bottom, _margin) });
  }
}
