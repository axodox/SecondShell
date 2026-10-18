#pragma once
#include "StatusBar.g.h"
#include "ActionLog.h"
#include "ActivityBars.h"
#include "AgentControl.h"
#include "SessionManager.h"
#include "SessionRenderer.h"
#include "SetupChecker.h"

namespace winrt::SecondShell::implementation
{
  //The status bar (docs/UIDesign.md, Status bar): the agent's activity and the step it is at, its current action, the
  //session, and the log with its flyout. It follows the services itself, on the UI thread.
  struct StatusBar : StatusBarT<StatusBar>
  {
    StatusBar();

    //On the UI thread, once.
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    void OnLogOpening(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& eventArgs);
    void OnLogClosed(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& eventArgs);

  private:
    std::shared_ptr<::SecondShell::Agent::ActionLog> _actionLog;
    std::shared_ptr<::SecondShell::Agent::AgentControl> _agentControl;
    std::shared_ptr<::SecondShell::Session::SessionManager> _sessions;
    std::shared_ptr<::SecondShell::Rendering::SessionRenderer> _renderer;
    std::shared_ptr<::SecondShell::Setup::SetupChecker> _setupChecker;
    std::vector<Axodox::Infrastructure::event_subscription> _subscriptions;
    Microsoft::UI::Dispatching::DispatcherQueue _queue{ nullptr };
    Microsoft::UI::Dispatching::DispatcherQueueTimer _timer{ nullptr };
    ::SecondShell::Ui::ActivityBars _bars;
    Microsoft::UI::Xaml::Media::Animation::Storyboard _flash{ nullptr };

    std::optional<::SecondShell::Agent::SequenceLogEntry> _sequence;  //the running or the last one, without its steps
    std::optional<::SecondShell::Agent::ActionLogEntry> _step;        //the running or the last one
    size_t _logCount = 0;
    bool _hasUnseenFailure = false;
    bool _isLogOpen = false;
    bool _isLogExpanded = false;

    void Post(std::function<void()> action);

    void ShowSequence(const ::SecondShell::Agent::SequenceLogEntry& sequence);
    void ShowStep(const ::SecondShell::Agent::ActionLogEntry& step);
    void ShowMode(::SecondShell::Agent::ControlMode mode);
    void UpdateActivity();
    void UpdateSession();
    void UpdateLog();
    void Flash();
    void SizeLog();
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct StatusBar : StatusBarT<StatusBar, implementation::StatusBar>
  {
  };
}
