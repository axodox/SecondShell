#pragma once
#include "LockScreenPolicy.h"
#include "StaThread.h"

namespace SecondShell::Session
{
  //A change of the host's own session, from WM_WTSSESSION_CHANGE.
  enum class OwnSessionChange
  {
    Locked,
    Unlocked,
    ConsoleConnected,     //now on this PC's screen: the user signed in at the PC
    ConsoleDisconnected,  //off this PC's screen: disconnected, or taken over by a Remote Desktop connection
    RemoteConnected,
    RemoteDisconnected
  };

  std::string_view to_string(OwnSessionChange change);

  //Reports the lock and connection changes of the host's own session (WTSRegisterSessionNotification, on a
  //message-only window of its own thread), and its state now.
  class SessionLockWatcher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SessionLockWatcher" };
    Axodox::Infrastructure::event_owner _events;

  public:
    SessionLockWatcher();
    ~SessionLockWatcher();

    OwnSessionState Current() const;

    //Raised on the watcher's thread.
    Axodox::Infrastructure::event_publisher<SessionLockWatcher*, OwnSessionChange> Changed;

  private:
    uint32_t _sessionId = 0;
    StaThread _thread;
    HWND _window = nullptr;  //used on _thread only

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void OnSessionChange(WPARAM kind);
  };
}
