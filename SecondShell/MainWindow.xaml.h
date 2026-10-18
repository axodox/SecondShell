#pragma once
#include "MainWindow.g.h"
#include "ActionLog.h"
#include "AgentControl.h"
#include "HostProtocol.h"
#include "InteractiveInputForwarder.h"
#include "SessionRenderer.h"

namespace winrt::SecondShell::implementation
{
  struct MainWindow : MainWindowT<MainWindow>
  {
    MainWindow();

    //The services behind the toolbar and the session view; before the window shows (Connect is taken by the XAML compiler).
    void UseServices(Axodox::Infrastructure::dependency_container* container);

    void ShowSessionState(::SecondShell::Protocol::SessionState state);
    void ShowControlMode(::SecondShell::Agent::ControlMode mode);

    //That the agent is in control in the title bar; its current action in the status bar, and the recent ones on hover.
    void ShowAgentRunning(bool isRunning);
    void ShowActionStarted(std::string_view summary);
    void ShowActionLogged(const ::SecondShell::Agent::ActionLogEntry& entry);

    void OnTakeControlClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnGiveBackClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);
    void OnStopClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

    //The user's input in the session view, which goes to the session while they have control.
    void OnSessionPointerMoved(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerPressed(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerReleased(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionPointerWheelChanged(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
    void OnSessionKeyDown(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::KeyRoutedEventArgs& eventArgs);
    void OnSessionKeyUp(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::KeyRoutedEventArgs& eventArgs);
    void OnSessionLostFocus(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& eventArgs);

  private:
    std::shared_ptr<::SecondShell::Agent::AgentControl> _agentControl;
    std::shared_ptr<::SecondShell::Agent::InteractiveInputForwarder> _inputForwarder;
    std::shared_ptr<::SecondShell::Rendering::SessionRenderer> _renderer;
    Microsoft::UI::Dispatching::DispatcherQueueTimer _agentIdleTimer{ nullptr };
    std::deque<std::wstring> _logLines;

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
