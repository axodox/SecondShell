#pragma once
#include "ActionLog.h"
#include "SessionRenderer.h"

namespace SecondShell::Ui
{
  //What the agent types, the keys it presses, the commands it runs and its clipboard actions, over the session view
  //(Design.md, Visualization): cards drawn as the status bar draws actions, in the bottom left corner of the session,
  //the newest at the bottom, the older ones moving up and fading out. A held key stays until it is released.
  class ActivityCards
  {
  public:
    //On the UI thread; the panel is the window's, over the session view.
    ActivityCards(Axodox::Infrastructure::dependency_container* container, const winrt::Microsoft::UI::Xaml::Controls::StackPanel& panel);

  private:
    struct Card
    {
      uint32_t Sequence = 0;
      uint32_t Step = 0;
      std::optional<std::string> HeldKey;
      std::chrono::steady_clock::time_point Shown;
      winrt::Microsoft::UI::Xaml::Controls::Border View{ nullptr };
      bool IsLeaving = false;
    };

    std::shared_ptr<Agent::ActionLog> _actionLog;
    std::shared_ptr<Rendering::SessionRenderer> _renderer;
    std::vector<Axodox::Infrastructure::event_subscription> _subscriptions;
    winrt::Microsoft::UI::Xaml::Controls::StackPanel _panel{ nullptr };
    winrt::Microsoft::UI::Dispatching::DispatcherQueue _queue{ nullptr };
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer _timer{ nullptr };
    std::deque<Card> _cards;

    void OnStarted(const Agent::ActionLogEntry& step);
    void OnFinished(const Agent::ActionLogEntry& step);

    void Add(const Agent::ActionLogEntry& step);
    void Release(const std::optional<std::string>& key);
    void Show(Card& card, const Agent::ActionLogEntry& step);
    void Leave(Card& card);
    void Tick();
    void Anchor();
  };
}
