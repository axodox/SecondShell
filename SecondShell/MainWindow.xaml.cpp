#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "ProductInfo.h"
#include "SequenceRunner.h"
#include "SessionDialogs.h"
#include "SessionManager.h"
#include "SetupChecker.h"
#include "StatusBar.xaml.h"

using namespace ::SecondShell::Actions;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Protocol;
using namespace ::SecondShell::Rendering;
using namespace ::SecondShell::Session;
using namespace ::SecondShell::Setup;
using namespace ::SecondShell::Ui;
using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;
using namespace winrt::Microsoft::UI::Input;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  //Agents send their sequences one after the other; so long after one the agent still counts as active
  const milliseconds _agentIdleShown{ 8000 };
  const double _agentIdleOpacity = 0.6;

  //Initialize-Session waits at most this long for the user to answer
  const minutes _existingSessionAnswerTimeout{ 5 };

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

    AppWindow().Closing({ this, &MainWindow::OnClosing });

    //E.g. Alt+Tab: the releases go to the next window, so the session would keep the keys held
    Activated([this](auto&&, const WindowActivatedEventArgs& eventArgs) {
      if (eventArgs.WindowActivationState() == WindowActivationState::Deactivated && _inputForwarder) _inputForwarder->ReleaseAll();
      });
  }

  void MainWindow::UseServices(dependency_container* container)
  {
    _actionLog = container->resolve<ActionLog>();
    _agentControl = container->resolve<AgentControl>();
    _inputForwarder = container->resolve<InteractiveInputForwarder>();
    _renderer = container->resolve<SessionRenderer>();
    get_self<implementation::StatusBar>(StatusBar())->UseServices(container);
    _activityCards = make_unique<ActivityCards>(container, ActivityCardsPanel());

    auto queue = DispatcherQueue();
    _modeChangedSubscription = _agentControl->ModeChanged([this, queue](AgentControl*, ControlMode mode) {
      queue.TryEnqueue([weak = get_weak(), mode] { if (auto self = weak.get()) self->ShowControlMode(mode); });
      });
    _runningChangedSubscription = container->resolve<SequenceRunner>()->RunningChanged([this, queue](SequenceRunner*, bool isRunning) {
      queue.TryEnqueue([weak = get_weak(), isRunning] { if (auto self = weak.get()) self->ShowAgentRunning(isRunning); });
      });

    _setupChecker = container->resolve<SetupChecker>();
    _setupLauncher = container->resolve<ElevatedSetupLauncher>();
    _setupChangedSubscription = _setupChecker->StatusChanged([this, queue](SetupChecker*, const SetupStatus&) {
      queue.TryEnqueue([weak = get_weak()] { if (auto self = weak.get()) self->ShowSessionState(self->_sessionState); });
      });

    //Asked once the window is up; until then a dialog has nowhere to show
    Content().as<FrameworkElement>().Loaded([weak = get_weak()](auto&&, auto&&) {
      auto self = weak.get();
      if (self && !self->_setupChecker->Status().IsReady()) self->OfferSetup();
      });

    auto sessions = container->resolve<SessionManager>();
    _stateChangedSubscription = sessions->StateChanged([this, queue](SessionManager*, SessionState state) {
      queue.TryEnqueue([weak = get_weak(), state] { if (auto self = weak.get()) self->ShowSessionState(state); });
      });
    ShowSessionState(sessions->Info().State);

    container->resolve<ExistingSessionPrompt>()->SetHandler([weak = get_weak()](const ExistingSession& session) -> std::optional<ExistingSessionPolicy> {
      auto self = weak.get();
      return self ? self->AskAboutExistingSession(session) : std::nullopt;
      });

    ShowControlMode(_agentControl->Mode());
  }

  void MainWindow::ShowSessionState(SessionState state)
  {
    struct Status
    {
      const wchar_t* Title;
      const wchar_t* Detail;
      bool IsWorking;
    };

    _sessionState = state;
    auto isSetupNeeded = !_setupChecker->Status().IsReady() && (state == SessionState::Stopped || state == SessionState::Failed);

    Status status;
    if (_isSettingUp)
    {
      status = { L"Setting up", L"Approve the administrator prompt to enable child sessions.", true };
    }
    else if (isSetupNeeded)
    {
      status = { L"One-time setup needed", _setupProblem.empty() ? L"Child sessions are not enabled on this PC yet; the agent's desktop cannot start until they are." : _setupProblem.c_str(), false };
    }
    else switch (state)
    {
    case SessionState::Connecting:
      status = { L"Starting the session", L"Connecting to the agent's desktop", true };
      break;
    case SessionState::SigningIn:
      status = { L"Starting the session", L"Signing in", true };
      break;
    case SessionState::StartingDriver:
      status = { L"Starting the session", L"Waiting for the desktop to finish starting; this can take a minute", true };
      break;
    case SessionState::Stopping:
      status = { L"Signing the session out", L"", true };
      break;
    case SessionState::Failed:
      status = { L"The session could not start", L"The agent can try again; the log has the details.", false };
      break;
    default:
      status = { L"No session yet", L"An AI agent starts one when it needs it.", false };
      break;
    }

    _isSessionReady = state == SessionState::Ready;
    SessionStatusPanel().Visibility(_isSessionReady ? Visibility::Collapsed : Visibility::Visible);
    SessionProgress().IsActive(status.IsWorking);
    SessionProgress().Visibility(status.IsWorking ? Visibility::Visible : Visibility::Collapsed);
    SessionStatusTitle().Text(status.Title);
    SessionStatusDetail().Text(status.Detail);
    SetUpButton().Visibility(isSetupNeeded && !_isSettingUp ? Visibility::Visible : Visibility::Collapsed);

    ShowControlMode(_agentControl->Mode());
  }

  void MainWindow::OnSetUpClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
  {
    OfferSetup();
  }

  fire_and_forget MainWindow::OfferSetup()
  {
    auto self = get_strong();
    if (_isSettingUp) co_return;

    auto smoothView = co_await SessionDialogs::AskToSetUp(Content().XamlRoot(), _setupChecker->Status());
    if (!smoothView) co_return;

    _isSettingUp = true;
    ShowSessionState(_sessionState);

    //The launcher waits for the elevated process; not on the UI thread
    SetupRequest request{ smoothView.Value() ? std::optional<bool>{ true } : std::nullopt };
    auto owner = Microsoft::UI::GetWindowFromWindowId(AppWindow().Id());
    auto queue = DispatcherQueue();
    co_await resume_background();
    auto outcome = _setupLauncher->Run(owner, request);
    auto status = _setupChecker->Check();
    co_await wil::resume_foreground(queue);

    _isSettingUp = false;
    if (status.IsReady()) _setupProblem.clear();
    else if (outcome == SetupOutcome::Declined) _setupProblem = L"The setup was cancelled. Child sessions need administrator rights once; choose Set up to try again.";
    else _setupProblem = L"The setup did not enable child sessions; the log has the details.";
    ShowSessionState(_sessionState);
  }

  void MainWindow::OnClosing(const Microsoft::UI::Windowing::AppWindow& /*sender*/, const AppWindowClosingEventArgs& eventArgs)
  {
    if (_isCloseConfirmed || !_actionLog) return;

    auto sequences = _actionLog->Sequences();
    if (sequences.empty() || sequences.back().Duration) return;

    eventArgs.Cancel(true);
    ConfirmClose(sequences.back());
  }

  fire_and_forget MainWindow::ConfirmClose(SequenceLogEntry sequence)
  {
    auto self = get_strong();
    if (!co_await SessionDialogs::AskToCloseWhileWorking(Content().XamlRoot(), sequence)) co_return;

    _isCloseConfirmed = true;
    _agentControl->Stop();
    Close();
  }

  std::optional<ExistingSessionPolicy> MainWindow::AskAboutExistingSession(const ExistingSession& session)
  {
    //The request waits on another thread while the user answers here
    auto answer = make_shared<promise<bool>>();
    auto isQueued = DispatcherQueue().TryEnqueue([weak = get_weak(), session, answer]() -> fire_and_forget {
      auto self = weak.get();
      if (!self)
      {
        answer->set_value(false);
        co_return;
      }
      answer->set_value(co_await SessionDialogs::AskToReset(self->Content().XamlRoot(), session));
      });

    auto future = answer->get_future();
    if (!isQueued || future.wait_for(_existingSessionAnswerTimeout) != future_status::ready || !future.get()) return std::nullopt;
    return ExistingSessionPolicy::Reset;
  }

  void MainWindow::ShowControlMode(ControlMode mode)
  {
    auto isAgent = mode == ControlMode::Agent;
    auto isUser = mode == ControlMode::User;
    TakeControlButton().Visibility(isUser ? Visibility::Collapsed : Visibility::Visible);
    TakeControlButton().IsEnabled(_isSessionReady);
    GiveBackButton().Visibility(isAgent ? Visibility::Collapsed : Visibility::Visible);
    StopButton().IsEnabled(_isSessionReady && isAgent);

    UserControlNotice().Visibility(isUser ? Visibility::Visible : Visibility::Collapsed);

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

  void MainWindow::OnSessionPointerGone(const IInspectable& /*sender*/, const PointerRoutedEventArgs& eventArgs)
  {
    //A mouse leaving the view keeps its buttons; a finger or the pen is gone (cancelled, or the pen out of range)
    if (!_inputForwarder->IsActive() || eventArgs.Pointer().PointerDeviceType() == PointerDeviceType::Mouse) return;

    auto point = eventArgs.GetCurrentPoint(SessionArea());
    if (eventArgs.Pointer().PointerDeviceType() == PointerDeviceType::Pen && point.IsInContact()) return;

    _inputForwarder->OnPointerGone(eventArgs.Pointer().PointerId());
    eventArgs.Handled(true);
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
    switch (eventArgs.Pointer().PointerDeviceType())
    {
    case PointerDeviceType::Touch:
      _inputForwarder->OnTouch(point, *transform);
      break;
    case PointerDeviceType::Pen:
      _inputForwarder->OnPen(point, *transform);
      break;
    default:
      _inputForwarder->OnPointerMoved({ point.Position().X, point.Position().Y }, *transform);
      if (auto change = ButtonChangeOf(point.Properties().PointerUpdateKind())) _inputForwarder->OnButton(change->Button, change->IsPressed);
    }

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
