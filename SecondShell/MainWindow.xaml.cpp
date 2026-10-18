#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "ProductInfo.h"

using namespace ::SecondShell::Actions;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Rendering;
using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;
using namespace winrt::Microsoft::UI::Input;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  const size_t _maxLogLines = 30;

  //Agents send their sequences one after the other; so long after one the agent still counts as active
  const milliseconds _agentIdleShown{ 8000 };
  const double _agentIdleOpacity = 0.6;

  namespace
  {
    struct ButtonChange
    {
      MouseButton Button;
      bool IsPressed;
    };

    //A press or release of a mouse button; a second button pressed while one is held comes with a pointer move
    std::optional<ButtonChange> ButtonChangeOf(PointerUpdateKind kind)
    {
      switch (kind)
      {
      case PointerUpdateKind::LeftButtonPressed:
        return ButtonChange{ MouseButton::Left, true };
      case PointerUpdateKind::LeftButtonReleased:
        return ButtonChange{ MouseButton::Left, false };
      case PointerUpdateKind::RightButtonPressed:
        return ButtonChange{ MouseButton::Right, true };
      case PointerUpdateKind::RightButtonReleased:
        return ButtonChange{ MouseButton::Right, false };
      case PointerUpdateKind::MiddleButtonPressed:
        return ButtonChange{ MouseButton::Middle, true };
      case PointerUpdateKind::MiddleButtonReleased:
        return ButtonChange{ MouseButton::Middle, false };
      default:
        return std::nullopt;
      }
    }

    wstring Outcome(const ActionLogEntry& entry)
    {
      switch (entry.Status)
      {
      case ActionStatus::Failed:
        return L" \u2014 failed: " + to_wstring(entry.Message);
      case ActionStatus::Cancelled:
        return L" \u2014 cancelled";
      default:
        return {};
      }
    }
  }

  MainWindow::MainWindow()
  {
    InitializeComponent();

    //The taskbar and Alt+Tab show the window's icon: the executable's own
    AppWindow().SetIcon(Microsoft::UI::GetIconIdFromIcon(LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1))));
    ExtendsContentIntoTitleBar(true);
    SetTitleBar(AppTitleBar());
    TitleText().Text(to_hstring(ProductInfo::Name()));
    VersionText().Text(to_hstring(ProductInfo::Version()));

    _agentIdleTimer = DispatcherQueue().CreateTimer();
    _agentIdleTimer.Interval(_agentIdleShown);
    _agentIdleTimer.IsRepeating(false);
    _agentIdleTimer.Tick([this](auto&&, auto&&) { AgentIndicator().Visibility(Visibility::Collapsed); });

    //E.g. Alt+Tab: the releases go to the next window, so the session would keep the keys held
    Activated([this](auto&&, const WindowActivatedEventArgs& eventArgs) {
      if (eventArgs.WindowActivationState() == WindowActivationState::Deactivated && _inputForwarder) _inputForwarder->ReleaseAll();
      });
  }

  void MainWindow::UseServices(dependency_container* container)
  {
    _agentControl = container->resolve<AgentControl>();
    _inputForwarder = container->resolve<InteractiveInputForwarder>();
    _renderer = container->resolve<SessionRenderer>();
    ShowControlMode(_agentControl->Mode());
  }

  void MainWindow::ShowSessionState(::SecondShell::Protocol::SessionState state)
  {
    StatusText().Text(to_hstring(std::format("Session: {}", Axodox::Infrastructure::to_string(state))));
  }

  void MainWindow::ShowControlMode(ControlMode mode)
  {
    auto isAgent = mode == ControlMode::Agent;
    auto isUser = mode == ControlMode::User;
    TakeControlButton().Visibility(isUser ? Visibility::Collapsed : Visibility::Visible);
    GiveBackButton().Visibility(isAgent ? Visibility::Collapsed : Visibility::Visible);
    StopButton().IsEnabled(isAgent);

    UserControlText().Visibility(isUser ? Visibility::Visible : Visibility::Collapsed);
    UserControlNotice().Visibility(isUser ? Visibility::Visible : Visibility::Collapsed);
    StoppedText().Visibility(mode == ControlMode::Stopped ? Visibility::Visible : Visibility::Collapsed);

    if (!isAgent)
    {
      _agentIdleTimer.Stop();
      AgentIndicator().Visibility(Visibility::Collapsed);
    }

    //So the keyboard goes to the session without a click into it first
    if (isUser) SessionArea().Focus(FocusState::Programmatic);
  }

  void MainWindow::ShowAgentRunning(bool isRunning)
  {
    if (!_agentControl->IsAgentAllowed()) return;

    AgentIndicator().Visibility(Visibility::Visible);
    AgentIndicator().Opacity(isRunning ? 1.0 : _agentIdleOpacity);
    AgentProgress().IsActive(isRunning);

    if (isRunning) _agentIdleTimer.Stop();
    else _agentIdleTimer.Start();
  }

  void MainWindow::ShowActionStarted(string_view summary)
  {
    ActionText().Text(L"\u25B6 " + to_wstring(summary));
  }

  void MainWindow::ShowActionLogged(const ActionLogEntry& entry)
  {
    auto text = to_wstring(entry.Summary) + Outcome(entry);
    ActionText().Text(text);

    //Oldest first, as a log reads
    auto time = zoned_time{ current_zone(), floor<seconds>(entry.Time) };
    _logLines.push_back(std::format(L"{:%H:%M:%S}  {}", time, text));
    if (_logLines.size() > _maxLogLines) _logLines.pop_front();

    wstring log;
    for (auto& line : _logLines) log += (log.empty() ? L"" : L"\n") + line;
    ActionLogText().Text(log);
  }

  void MainWindow::OnTakeControlClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _agentControl->TakeControl();
  }

  void MainWindow::OnGiveBackClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _agentControl->GiveBackControl();
  }

  void MainWindow::OnStopClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _agentControl->Stop();
  }

  void MainWindow::OnSessionPointerMoved(const IInspectable& /*sender*/, const PointerRoutedEventArgs& eventArgs)
  {
    ForwardPointer(eventArgs);
  }

  void MainWindow::OnSessionPointerPressed(const IInspectable& /*sender*/, const PointerRoutedEventArgs& eventArgs)
  {
    if (!ForwardPointer(eventArgs)) return;

    //The release comes here even off the view, so no button stays held in the session
    SessionArea().Focus(FocusState::Pointer);
    SessionArea().CapturePointer(eventArgs.Pointer());
  }

  void MainWindow::OnSessionPointerReleased(const IInspectable& /*sender*/, const PointerRoutedEventArgs& eventArgs)
  {
    if (ForwardPointer(eventArgs)) SessionArea().ReleasePointerCapture(eventArgs.Pointer());
  }

  void MainWindow::OnSessionPointerWheelChanged(const IInspectable& /*sender*/, const PointerRoutedEventArgs& eventArgs)
  {
    if (!ForwardPointer(eventArgs)) return;

    auto properties = eventArgs.GetCurrentPoint(SessionArea()).Properties();
    _inputForwarder->OnWheel(float(properties.MouseWheelDelta()) / WHEEL_DELTA, properties.IsHorizontalMouseWheel() ? WheelAxis::Horizontal : WheelAxis::Vertical);
  }

  void MainWindow::OnSessionKeyDown(const IInspectable& /*sender*/, const KeyRoutedEventArgs& eventArgs)
  {
    ForwardKey(eventArgs, true);
  }

  void MainWindow::OnSessionKeyUp(const IInspectable& /*sender*/, const KeyRoutedEventArgs& eventArgs)
  {
    ForwardKey(eventArgs, false);
  }

  void MainWindow::OnSessionLostFocus(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    _inputForwarder->ReleaseAll();
  }

  bool MainWindow::ForwardPointer(const PointerRoutedEventArgs& eventArgs)
  {
    if (!_inputForwarder->IsActive()) return false;

    auto transform = _renderer->Transform();
    if (!transform) return false;

    auto point = eventArgs.GetCurrentPoint(SessionArea());
    _inputForwarder->OnPointerMoved({ point.Position().X, point.Position().Y }, *transform);
    if (auto change = ButtonChangeOf(point.Properties().PointerUpdateKind())) _inputForwarder->OnButton(change->Button, change->IsPressed);

    eventArgs.Handled(true);
    return true;
  }

  void MainWindow::ForwardKey(const KeyRoutedEventArgs& eventArgs, bool isPressed)
  {
    if (!_inputForwarder->IsActive()) return;

    //Handled, so Tab and the arrows do not move the focus out of the view
    _inputForwarder->OnKey(uint16_t(eventArgs.Key()), isPressed);
    eventArgs.Handled(true);
  }
}
