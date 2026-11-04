#pragma once
#include "IInputAvailability.h"
#include "LockScreenPolicy.h"
#include "LoopbackClient.h"
#include "MachineConfiguration.h"
#include "SessionEventLog.h"
#include "SessionLockWatcher.h"
#include "SessionManager.h"
#include "SettingsStore.h"

namespace SecondShell::Session
{
  //Keeps the user's session unlocked behind the lock screen while the agent works, if the user turned it on (Design.md,
  //Working while the screen is locked): when the user locks the PC, it signs the session in again through the loopback
  //connection, and when the user signs in at the PC, Windows ends that connection. LockScreenPolicy decides; this class
  //feeds it and carries its decisions out, all on a thread of its own.
  class LockScreenBridge
  {
    inline static const Axodox::Infrastructure::logger _logger{ "LockScreenBridge" };

  public:
    explicit LockScreenBridge(Axodox::Infrastructure::dependency_container* container);
    ~LockScreenBridge();

    LockScreenBridge(const LockScreenBridge&) = delete;
    LockScreenBridge& operator=(const LockScreenBridge&) = delete;

    //Input reaches the child session exactly while the user's session is unlocked, wherever it is shown.
    InputStatus Status() const;

  private:
    enum class Signal
    {
      ActiveChanged,
      Locked,
      Unlocked,
      ConsoleConnected,
      OtherChange,
      LoopbackConnected,
      LoopbackDisconnected
    };

    std::shared_ptr<SettingsStore> _settings;
    std::shared_ptr<SessionManager> _sessions;
    std::shared_ptr<Setup::IMachineConfiguration> _machine;
    std::shared_ptr<SessionLockWatcher> _watcher;
    std::shared_ptr<LoopbackClient> _loopback;
    std::shared_ptr<Agent::SessionEventLog> _eventLog;  //told of each change of the status

    std::mutex _mutex;
    std::condition_variable _signalled;
    std::deque<Signal> _signals;
    bool _isStopping = false;
    std::thread _thread;

    LockScreenPolicy _policy;  //used on _thread only
    std::atomic_bool _isResuming = false;  //_policy.IsResuming(), for other threads
    InputStatus _reportedStatus;  //used on _thread only

    Axodox::Infrastructure::event_subscription _settingsSubscription;
    Axodox::Infrastructure::event_subscription _stateSubscription;
    Axodox::Infrastructure::event_subscription _watcherSubscription;
    Axodox::Infrastructure::event_subscription _loginSubscription;
    Axodox::Infrastructure::event_subscription _disconnectSubscription;

    void Post(Signal signal);
    void Run();
    void Handle(Signal signal);
    void Execute(LockScreenAction action);
    void Connect();
    bool ShouldBeActive() const;
  };

  //IInputAvailability from the bridge's status, so the bridge stays one instance however it is resolved.
  class LockScreenInputAvailability : public IInputAvailability
  {
  public:
    explicit LockScreenInputAvailability(Axodox::Infrastructure::dependency_container* container);

    virtual Protocol::InputAvailability Current() const override;

  private:
    std::shared_ptr<LockScreenBridge> _bridge;
  };
}
