#include "pch.h"
#include "LockScreenBridge.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Threading;
using namespace SecondShell::Protocol;
using namespace SecondShell::Setup;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Session
{
  LockScreenBridge::LockScreenBridge(dependency_container* container) :
    _settings(container->resolve<SettingsStore>()),
    _sessions(container->resolve<SessionManager>()),
    _machine(container->resolve<IMachineConfiguration>()),
    _watcher(container->resolve<SessionLockWatcher>()),
    _loopback(container->resolve<LoopbackClient>())
  {
    _thread = thread([this] { Run(); });

    _settingsSubscription = _settings->Changed([this](SettingsStore*, const Settings&) { Post(Signal::ActiveChanged); });
    _stateSubscription = _sessions->StateChanged([this](SessionManager*, SessionState) { Post(Signal::ActiveChanged); });
    _loginSubscription = _loopback->LoginCompleted([this](LoopbackClient*) { Post(Signal::LoopbackConnected); });
    _disconnectSubscription = _loopback->Disconnected([this](LoopbackClient*, long) { Post(Signal::LoopbackDisconnected); });
    _watcherSubscription = _watcher->Changed([this](SessionLockWatcher*, OwnSessionChange change) {
      switch (change)
      {
      case OwnSessionChange::Locked:
      case OwnSessionChange::ConsoleDisconnected:
        Post(Signal::Locked);
        break;
      case OwnSessionChange::Unlocked:
        Post(Signal::Unlocked);
        break;
      case OwnSessionChange::ConsoleConnected:
        Post(Signal::ConsoleConnected);
        break;
      default:
        break;
      }
      });

    Post(Signal::ActiveChanged);
  }

  LockScreenBridge::~LockScreenBridge()
  {
    //No more signals from here on
    _settingsSubscription.reset();
    _stateSubscription.reset();
    _watcherSubscription.reset();
    _loginSubscription.reset();
    _disconnectSubscription.reset();

    {
      lock_guard lock{ _mutex };
      _isStopping = true;
    }
    _signalled.notify_all();
    _thread.join();
  }

  void LockScreenBridge::Post(Signal signal)
  {
    {
      lock_guard lock{ _mutex };
      _signals.push_back(signal);
    }
    _signalled.notify_all();
  }

  void LockScreenBridge::Run()
  {
    set_thread_name("* Lock screen bridge");

    while (true)
    {
      optional<Signal> signal;
      {
        unique_lock lock{ _mutex };
        auto isWoken = [this] { return _isStopping || !_signals.empty(); };
        if (auto check = _policy.NextCheck()) _signalled.wait_until(lock, *check, isWoken);
        else _signalled.wait(lock, isWoken);

        if (_isStopping) break;
        if (!_signals.empty())
        {
          signal = _signals.front();
          _signals.pop_front();
        }
      }

      try
      {
        if (signal) Handle(*signal);
        if (auto check = _policy.NextCheck(); check && *check <= steady_clock::now())
        {
          Execute(_policy.OnCheck(steady_clock::now(), _watcher->Current()));
        }
      }
      catch (const winrt::hresult_error& error)
      {
        _logger.log(log_severity::error, "Keeping the session unlocked failed: {}", winrt::to_string(error.message()));
      }
      catch (const exception& exception)
      {
        _logger.log(log_severity::error, "Keeping the session unlocked failed: {}", exception.what());
      }

      _isResuming = _policy.IsResuming();
    }

    //Ending the connection leaves the session disconnected, so it is locked again
    if (_policy.IsLoopbackActive()) _loopback->Disconnect();
  }

  void LockScreenBridge::Handle(Signal signal)
  {
    auto now = steady_clock::now();
    switch (signal)
    {
    case Signal::ActiveChanged:
    {
      auto isActive = ShouldBeActive();
      if (isActive != _policy.IsActive()) _logger.log(log_severity::information, isActive ?
        "On: the session will be kept unlocked behind the lock screen while the agent's session runs." :
        "Off: the session locks as usual.");
      Execute(_policy.SetActive(isActive, now, _watcher->Current()));
      break;
    }
    case Signal::Locked:
      _policy.OnLocked(now);
      break;
    case Signal::Unlocked:
      _policy.OnUnlocked(_watcher->Current());
      break;
    case Signal::ConsoleConnected:
      Execute(_policy.OnConsoleConnected());
      break;
    case Signal::LoopbackConnected:
      _policy.OnLoopbackConnected();
      break;
    case Signal::LoopbackDisconnected:
      _loopback->Release();
      _policy.OnLoopbackDisconnected(now, _watcher->Current());
      //Also when the user signs in at the PC: Windows may end the connection before it reports the sign-in
      if (auto retry = _policy.NextCheck()) _logger.log(log_severity::information, "The connection ended; connecting again in {} unless the user signs in at the PC meanwhile.", duration_cast<seconds>(*retry - now));
      break;
    }
  }

  void LockScreenBridge::Execute(LockScreenAction action)
  {
    switch (action)
    {
    case LockScreenAction::Connect:
      Connect();
      break;
    case LockScreenAction::Disconnect:
      _logger.log(log_severity::information, "Ending the connection to this PC's own session.");
      _loopback->Disconnect();
      break;
    default:
      break;
    }
  }

  void LockScreenBridge::Connect()
  {
    auto now = steady_clock::now();
    if (!_machine->IsRemoteDesktopEnabled())
    {
      _logger.log(log_severity::warning, "The screen is locked, but Remote Desktop is turned off on this PC, so the agent's input stops until the user signs in again.");
      _policy.OnConnectFailed(now, false);
      return;
    }

    _logger.log(log_severity::information, "The screen is locked: signing this PC's own session in again behind the lock screen, so the agent's input keeps working.");
    try
    {
      if (_loopback->Connect()) return;

      _logger.log(log_severity::warning, "No password is saved, so the session cannot be kept unlocked; the agent's input stops until the user signs in again.");
      _policy.OnConnectFailed(now, false);
    }
    catch (...)
    {
      _policy.OnConnectFailed(now, true);
      throw;
    }
  }

  InputStatus LockScreenBridge::Status() const
  {
    auto state = _watcher->Current();
    return {
      .IsUserPresent = !state.IsLocked && state.IsConnected && state.IsOnConsole,
      .Input = !state.IsLocked ? InputAvailability::Available : _isResuming ? InputAvailability::Resuming : InputAvailability::Unavailable
    };
  }

  bool LockScreenBridge::ShouldBeActive() const
  {
    return _settings->Current().WorkWhileLocked && _sessions->Info().State == SessionState::Ready;
  }

  LockScreenInputAvailability::LockScreenInputAvailability(dependency_container* container) :
    _bridge(container->resolve<LockScreenBridge>())
  { }

  InputAvailability LockScreenInputAvailability::Current() const
  {
    return _bridge->Status().Input;
  }
}
