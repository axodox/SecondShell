#pragma once
#include "MainWindow.g.h"
#include "ActionLog.h"
#include "ActivityCards.h"
#include "AgentControl.h"
#include "ElevatedSetupLauncher.h"
#include "ExistingSessionPrompt.h"
#include "InteractiveInputForwarder.h"
#include "SessionRenderer.h"
#include "SetupChecker.h"

namespace winrt::SecondShell::implementation
{
  struct MainWindow : MainWindowT<MainWindow>
  {
    MainWindow();

    //The services behind the toolbar and the session view; before the window shows (Connect is taken by the XAML compiler).
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    void ShowControlMode(::SecondShell::Agent::ControlMode mode);

    //Until the session is ready: what is happening, in the view, and the toolbar disabled.
    void ShowSessionState(::SecondShell::Protocol::SessionState state);

    //That the agent is in control, in the title bar.
    void ShowAgentRunning(bool isRunning);

    void OnTakeControlClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnGiveBackClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnStopClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnSetUpClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnSettingsClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnBackClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

    //The user's input in the session view, which goes to the session while they have control.
    void OnSessionPointerMoved(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerPressed(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerReleased(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerWheelChanged(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerGone(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionKeyDown(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::KeyRoutedEventArgs& eventArgs);
    void OnSessionKeyUp(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::KeyRoutedEventArgs& eventArgs);
    void OnSessionLostFocus(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    std::shared_ptr<::SecondShell::Agent::ActionLog> _actionLog;
    std::shared_ptr<::SecondShell::Agent::AgentControl> _agentControl;
    std::shared_ptr<::SecondShell::Agent::InteractiveInputForwarder> _inputForwarder;
    std::shared_ptr<::SecondShell::Rendering::SessionRenderer> _renderer;
    std::unique_ptr<::SecondShell::Ui::ActivityCards> _activityCards;
    std::shared_ptr<::SecondShell::Setup::SetupChecker> _setupChecker;
    std::shared_ptr<::SecondShell::Setup::ElevatedSetupLauncher> _setupLauncher;
    Axodox::Infrastructure::event_subscription _setupChangedSubscription;
    ::SecondShell::Protocol::SessionState _sessionState = ::SecondShell::Protocol::SessionState::Stopped;
    bool _isSettingUp = false;
    std::wstring _setupProblem;  //why the last setup did not happen
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    Axodox::Infrastructure::event_subscription _stateChangedSubscription;
    bool _isSessionReady = false;
    Axodox::Infrastructure::event_subscription _runningChangedSubscription;
    Microsoft::UI::Dispatching::DispatcherQueueTimer _agentIdleTimer{ nullptr };

    bool _isCloseConfirmed = false;

    //The settings in place of the session view, or the session view again.
    void ShowSettings(bool isShown);

    //The one-time setup (Design.md, Repair): asked when the window opens and from Set up; runs elevated.
    fire_and_forget OfferSetup();

    //Closing while the agent works asks first (docs/UIDesign.md, Dialogs).
    void OnClosing(const Microsoft::UI::Windowing::AppWindow& sender, const Microsoft::UI::Windowing::AppWindowClosingEventArgs& eventArgs);
    fire_and_forget ConfirmClose(::SecondShell::Agent::SequenceLogEntry sequence);

    //What to do with a child session from before, asked in a dialog; on the caller's thread.
    std::optional<::SecondShell::Protocol::ExistingSessionPolicy> AskAboutExistingSession(const ::SecondShell::Session::ExistingSession& session);

    bool ForwardPointer(const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void ForwardKey(const Microsoft::UI::Xaml::Input::KeyRoutedEventArgs& eventArgs, bool isPressed);
  };
}

namespace winrt::SecondShell::factory_implementation
{
  struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
  {
  };
}
