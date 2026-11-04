#include "pch.h"
#include "LockScreenPolicy.h"

using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    //Nobody uses the session at this PC: it is locked on the PC's screen, or not connected at all. Locked under
    //someone's own Remote Desktop connection, it is theirs, and a loopback connection would throw them out.
    bool IsAway(const OwnSessionState& state)
    {
      return (state.IsLocked && state.IsOnConsole) || !state.IsConnected;
    }
  }

  LockScreenAction LockScreenPolicy::SetActive(bool isActive, clock::time_point now, const OwnSessionState& state)
  {
    if (isActive == _isActive) return LockScreenAction::None;
    _isActive = isActive;

    if (!isActive)
    {
      _checkAt.reset();
      _isAway = false;
      _retries = 0;
      if (_loopback == LoopbackState::Idle) return LockScreenAction::None;

      _loopback = LoopbackState::Idle;
      return LockScreenAction::Disconnect;
    }

    //Turned on while the user is already away, e.g. the agent started the session after the user left
    if (_isArmed && IsAway(state)) _checkAt = now + LockDelay;
    return LockScreenAction::None;
  }

  bool LockScreenPolicy::IsActive() const
  {
    return _isActive;
  }

  void LockScreenPolicy::OnLocked(clock::time_point now)
  {
    if (!_isActive || !_isArmed || _loopback != LoopbackState::Idle) return;
    _checkAt = now + LockDelay;
  }

  void LockScreenPolicy::OnUnlocked(const OwnSessionState& state)
  {
    //The loopback connection unlocks the session too, but not on this PC's screen
    if (!state.IsOnConsole && _loopback != LoopbackState::Idle) return;

    _isArmed = true;
    _isAway = false;
    _retries = 0;
    _checkAt.reset();
  }

  LockScreenAction LockScreenPolicy::OnConsoleConnected()
  {
    _isAway = false;
    _checkAt.reset();
    if (_loopback == LoopbackState::Idle) return LockScreenAction::None;

    //Windows ends it anyway; this makes sure
    _loopback = LoopbackState::Idle;
    return LockScreenAction::Disconnect;
  }

  void LockScreenPolicy::OnLoopbackConnected()
  {
    if (_loopback == LoopbackState::Connecting) _loopback = LoopbackState::Connected;
    _retries = 0;
  }

  void LockScreenPolicy::OnLoopbackDisconnected(clock::time_point now, const OwnSessionState& state)
  {
    _loopback = LoopbackState::Idle;

    //Ended by the user signing in at this PC: nothing to do
    if (!_isAway || (state.IsConnected && state.IsOnConsole && !state.IsLocked)) return;
    Retry(now);
  }

  void LockScreenPolicy::OnConnectFailed(clock::time_point now, bool canRetry)
  {
    _loopback = LoopbackState::Idle;
    if (canRetry) Retry(now);
    else _checkAt.reset();
  }

  optional<LockScreenPolicy::clock::time_point> LockScreenPolicy::NextCheck() const
  {
    return _checkAt;
  }

  LockScreenAction LockScreenPolicy::OnCheck(clock::time_point now, const OwnSessionState& state)
  {
    if (!_checkAt || now < *_checkAt) return LockScreenAction::None;
    _checkAt.reset();

    if (!_isActive || _loopback != LoopbackState::Idle || !IsAway(state)) return LockScreenAction::None;
    if (!_isArmed && !_isAway) return LockScreenAction::None;

    _isArmed = false;
    _isAway = true;
    _loopback = LoopbackState::Connecting;
    return LockScreenAction::Connect;
  }

  bool LockScreenPolicy::IsLoopbackActive() const
  {
    return _loopback != LoopbackState::Idle;
  }

  bool LockScreenPolicy::IsResuming() const
  {
    return _isActive && (_checkAt.has_value() || _loopback != LoopbackState::Idle);
  }

  void LockScreenPolicy::Retry(clock::time_point now)
  {
    if (!_isActive || !_isAway || _retries >= MaxRetries)
    {
      _checkAt.reset();
      return;
    }

    _retries++;
    _checkAt = now + RetryDelay;
  }
}
