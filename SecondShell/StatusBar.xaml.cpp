#include "pch.h"
#include "StatusBar.xaml.h"
#if __has_include("StatusBar.g.cpp")
#include "StatusBar.g.cpp"
#endif
#include "ActionLogPanel.xaml.h"
#include "ActionView.h"

using namespace ::SecondShell::Actions;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Protocol;
using namespace ::SecondShell::Rendering;
using namespace ::SecondShell::Session;
using namespace ::SecondShell::Setup;
using namespace ::SecondShell::Ui;
using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::Windows::Foundation;

namespace winrt::SecondShell::implementation
{
  //In UTF-8, whatever the compiler's execution character set
  const char* const _separator = " \xC2\xB7 ";
  const char* const _times = " \xC3\x97 ";

  const milliseconds _tick{ 500 };
  const seconds _thinkingShown{ 8 };      //agents send their sequences one after the other
  const seconds _longStep{ 2 };           //from here a step shows how long it runs
  const milliseconds _flashDuration{ 2400 };

  //Above the status bar and the title bar, with a margin
  const double _logWidth = 760;
  const double _logHeight = 720;
  const double _logMargin = 24;
  const double _windowChrome = 40 + 38 + 16;

  StatusBar::StatusBar()
  {
    InitializeComponent();
    _bars = ActivityBars{ Bars() };
    _bars.Show(Activity::Idle);

    auto panel = get_self<ActionLogPanel>(LogPanel());
    panel->CloseRequested = [this] { LogFlyout().Hide(); };
    panel->ExpandChanged = [this](bool isExpanded) {
      _isLogExpanded = isExpanded;
      SizeLog();
      };
  }

  void StatusBar::UseServices(dependency_container* container)
  {
    _actionLog = container->resolve<ActionLog>();
    _agentControl = container->resolve<AgentControl>();
    _sessions = container->resolve<SessionManager>();
    _renderer = container->resolve<SessionRenderer>();
    _setupChecker = container->resolve<SetupChecker>();
    _queue = DispatcherQueue();

    _subscriptions.push_back(_actionLog->SequenceChanged([this](ActionLog*, const SequenceLogEntry& sequence) { Post([this, sequence] { ShowSequence(sequence); }); }));
    _subscriptions.push_back(_actionLog->ActionStarted([this](ActionLog*, const ActionLogEntry& step) { Post([this, step] { ShowStep(step); }); }));
    _subscriptions.push_back(_actionLog->EntryAdded([this](ActionLog*, const ActionLogEntry& step) { Post([this, step] { ShowStep(step); }); }));
    _subscriptions.push_back(_agentControl->ModeChanged([this](AgentControl*, ControlMode mode) { Post([this, mode] { ShowMode(mode); }); }));
    _subscriptions.push_back(_sessions->StateChanged([this](SessionManager*, SessionState) { Post([this] { UpdateSession(); }); }));

    //Elapsed times, the agent going idle, and the frame rate
    _timer = _queue.CreateTimer();
    _timer.Interval(_tick);
    _timer.Tick([this](auto&&, auto&&) {
      UpdateActivity();
      UpdateSession();
      });
    _timer.Start();

    ShowMode(_agentControl->Mode());
    UpdateSession();
  }

  void StatusBar::OnLogOpening(const IInspectable& /*sender*/, const IInspectable& /*eventArgs*/)
  {
    _isLogOpen = true;
    _hasUnseenFailure = false;
    LogButton().Background(ActionView::Resource<Brush>(L"LogOpenBrush"));
    LogButton().BorderBrush(ActionView::Resource<Brush>(L"LogOpenBorderBrush"));
    SizeLog();
    UpdateLog();
  }

  void StatusBar::OnLogClosed(const IInspectable& /*sender*/, const IInspectable& /*eventArgs*/)
  {
    _isLogOpen = false;
    LogButton().ClearValue(Controls::Control::BackgroundProperty());
    LogButton().ClearValue(Controls::Control::BorderBrushProperty());
  }

  void StatusBar::Post(function<void()> action)
  {
    _queue.TryEnqueue([weak = get_weak(), action = move(action)] {
      if (weak.get()) action();
      });
  }

  void StatusBar::ShowSequence(const SequenceLogEntry& sequence)
  {
    _sequence = sequence;
    UpdateActivity();
    if (_isLogOpen) UpdateLog();
  }

  void StatusBar::ShowStep(const ActionLogEntry& step)
  {
    auto isNew = !step.Status;
    _step = step;

    ActionSeparator().Visibility(Visibility::Visible);
    ActionPill().Visibility(Visibility::Visible);
    ActionHost().Child(ActionView::Build(step));

    auto outcome = ActionOutcomeText();
    outcome.Text(L"");
    if (step.Status == ActionStatus::Failed)
    {
      outcome.Text(to_hstring("failed: " + step.Message));
      outcome.Foreground(ActionView::Resource<Brush>(L"FailureBrush"));
    }
    else if (step.Status == ActionStatus::Cancelled)
    {
      outcome.Text(L"cancelled");
      outcome.Foreground(ActionView::Resource<Brush>(L"MutedTextBrush"));
    }

    if (isNew)
    {
      Flash();
    }
    else
    {
      _logCount++;
      if (step.Status == ActionStatus::Failed)
      {
        if (_flash) _flash.Stop();
        ActionPillBrush().Color(ActionView::Resource<SolidColorBrush>(L"FailedActionBrush").Color());
        _hasUnseenFailure = !_isLogOpen;
      }
    }

    UpdateActivity();
    UpdateLog();
  }

  void StatusBar::ShowMode(ControlMode mode)
  {
    AgentActivity().Visibility(mode == ControlMode::Agent ? Visibility::Visible : Visibility::Collapsed);
    UserActivity().Visibility(mode == ControlMode::User ? Visibility::Visible : Visibility::Collapsed);
    StoppedActivity().Visibility(mode == ControlMode::Stopped ? Visibility::Visible : Visibility::Collapsed);
  }

  void StatusBar::UpdateActivity()
  {
    if (!_sequence)
    {
      _bars.Show(Activity::Idle);
      return;
    }

    auto now = system_clock::now();
    if (!_sequence->Duration)
    {
      _bars.Show(Activity::Working);
      ActivityText().Text(to_hstring(_sequence->Description.empty() ? "Agent working" : _sequence->Description));
      ActivityText().ClearValue(Controls::TextBlock::ForegroundProperty());

      string detail;
      if (_step && _step->Sequence == _sequence->Number)
      {
        detail = std::format("step {} of {}", _step->Step + 1, _sequence->StepCount);
        auto elapsed = now - _step->Time;
        if (!_step->Status && elapsed >= _longStep) detail += _separator + FormatDuration(duration_cast<seconds>(elapsed));
      }
      ActivityDetailText().Text(to_hstring(detail));
      return;
    }

    //After a sequence the agent is likely to send the next one soon, unless this one failed
    auto ended = _sequence->Time + *_sequence->Duration;
    auto isThinking = now - ended < _thinkingShown && !_sequence->Error;
    _bars.Show(isThinking ? Activity::Thinking : Activity::Idle);
    ActivityText().Text(isThinking ? L"Agent thinking" : L"Agent idle");
    ActivityText().Foreground(ActionView::Resource<Brush>(L"SessionTextBrush"));
    ActivityDetailText().Text(isThinking ? to_hstring(std::format("last step {} s ago", duration_cast<seconds>(now - ended).count())) : L"");
  }

  void StatusBar::UpdateSession()
  {
    auto info = _sessions->Info();
    if (info.State != SessionState::Ready || !info.SessionId || !info.Display)
    {
      switch (info.State)
      {
      case SessionState::Connecting:
      case SessionState::SigningIn:
      case SessionState::StartingDriver:
        SessionText().Text(L"Session starting");
        break;
      case SessionState::Stopping:
        SessionText().Text(L"Session signing out");
        break;
      case SessionState::Failed:
        SessionText().Text(L"Session failed to start");
        break;
      default:
        SessionText().Text(_setupChecker->Status().IsReady() ? L"No session" : L"Setup needed: child sessions are not enabled");
        break;
      }
      return;
    }

    auto text = std::format("Session {}{}{}{}{} at {} %", *info.SessionId, _separator, info.Display->Width, _times, info.Display->Height, info.Display->Scale);
    if (auto rate = _renderer->FrameRate()) text += std::format("{}{:.0f} fps", _separator, *rate);
    SessionText().Text(to_hstring(text));
  }

  void StatusBar::UpdateLog()
  {
    LogCountText().Text(to_hstring(_logCount));
    LogFailureDot().Visibility(_hasUnseenFailure ? Visibility::Visible : Visibility::Collapsed);
    if (_isLogOpen) get_self<ActionLogPanel>(LogPanel())->Show(_actionLog->Sequences());
  }

  void StatusBar::Flash()
  {
    if (_flash) _flash.Stop();

    ColorAnimation animation;
    animation.From(ActionView::Resource<IReference<Windows::UI::Color>>(L"ActionFlashColor").Value());
    animation.To(ActionView::Resource<IReference<Windows::UI::Color>>(L"ActionRestColor").Value());
    animation.Duration(DurationHelper::FromTimeSpan(_flashDuration));
    animation.EnableDependentAnimation(true);
    ExponentialEase ease;
    ease.EasingMode(EasingMode::EaseOut);
    animation.EasingFunction(ease);
    Storyboard::SetTarget(animation, ActionPillBrush());
    Storyboard::SetTargetProperty(animation, L"Color");

    _flash = Storyboard{};
    _flash.Children().Append(animation);
    _flash.Begin();
  }

  void StatusBar::SizeLog()
  {
    auto root = XamlRoot();
    if (!root) return;

    auto size = root.Size();
    auto panel = LogPanel();
    panel.Width(min(_logWidth, size.Width - _logMargin));
    panel.Height(_isLogExpanded ? size.Height - _windowChrome : min(_logHeight, size.Height - _windowChrome));
  }
}
