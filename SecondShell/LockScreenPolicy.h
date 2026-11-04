#pragma once

namespace SecondShell::Session
{
  //The host's own session, the one the user signs in to, as WTS reports it.
  struct OwnSessionState
  {
    bool IsLocked = false;
    bool IsConnected = true;  //to this PC's screen or to a Remote Desktop client; false while disconnected
    bool IsOnConsole = true;  //on this PC's screen, not through a Remote Desktop client
  };

  enum class LockScreenAction
  {
    None,
    Connect,    //the loopback connection, which unlocks the user's session behind the lock screen
    Disconnect
  };

  //When to keep the user's session unlocked behind the lock screen, so the agent's input still reaches the child
  //session (Design.md, Working while the screen is locked; Findings.md 2.17). No Windows calls: LockScreenBridge feeds
  //it the events and the session's state, and carries out what it returns. Used on one thread.
  //
  //It connects only after a lock the user made at this PC: it is armed by an unlock and disarmed by its own
  //connection, so the brief lock while the user signs back in, and the unlock its own connection causes, change
  //nothing.
  class LockScreenPolicy
  {
  public:
    using clock = std::chrono::steady_clock;

    //From the lock to the connection: a lock the user takes back at once (or a lock screen flashing by) changes nothing.
    static constexpr std::chrono::seconds LockDelay{ 5 };
    //After the connection ends while the user is still away, or could not be made.
    static constexpr std::chrono::seconds RetryDelay{ 15 };
    static constexpr uint32_t MaxRetries = 3;

    //Active while the user wants it and the agent's session is running. Turning off ends the connection.
    LockScreenAction SetActive(bool isActive, clock::time_point now, const OwnSessionState& state);
    bool IsActive() const;

    //The session locked, or was disconnected.
    void OnLocked(clock::time_point now);

    //The session unlocked: by the user at this PC (which arms the policy again), or by the loopback connection.
    void OnUnlocked(const OwnSessionState& state);

    //The user signed in at this PC, which takes the session over from any Remote Desktop connection.
    LockScreenAction OnConsoleConnected();

    void OnLoopbackConnected();
    void OnLoopbackDisconnected(clock::time_point now, const OwnSessionState& state);

    //The connection could not be started; without retrying when it cannot work (no saved password, Remote Desktop off).
    void OnConnectFailed(clock::time_point now, bool canRetry);

    //When OnCheck is due next, if at all.
    std::optional<clock::time_point> NextCheck() const;
    LockScreenAction OnCheck(clock::time_point now, const OwnSessionState& state);

    //Whether the loopback connection is starting or up.
    bool IsLoopbackActive() const;

    //Whether a locked session is about to be unlocked behind the lock screen: a connection is due, starting or up.
    bool IsResuming() const;

  private:
    enum class LoopbackState
    {
      Idle,
      Connecting,
      Connected
    };

    bool _isActive = false;
    bool _isArmed = true;   //the user is at this PC; their next lock is one to act on
    bool _isAway = false;   //connected for the user's lock, and they have not signed in again
    LoopbackState _loopback = LoopbackState::Idle;
    std::optional<clock::time_point> _checkAt;
    uint32_t _retries = 0;

    void Retry(clock::time_point now);
  };
}
