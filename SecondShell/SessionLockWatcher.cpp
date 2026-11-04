#include "pch.h"
#include "SessionLockWatcher.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    const wchar_t* const _windowClassName = L"SecondShell.SessionLockWatcher";
  }

  string_view to_string(OwnSessionChange change)
  {
    switch (change)
    {
    case OwnSessionChange::Locked: return "locked";
    case OwnSessionChange::Unlocked: return "unlocked";
    case OwnSessionChange::ConsoleConnected: return "connected to this PC's screen";
    case OwnSessionChange::ConsoleDisconnected: return "disconnected from this PC's screen";
    case OwnSessionChange::RemoteConnected: return "connected through Remote Desktop";
    case OwnSessionChange::RemoteDisconnected: return "disconnected from Remote Desktop";
    default: return "changed";
    }
  }

  SessionLockWatcher::SessionLockWatcher() :
    Changed(_events)
  {
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    _sessionId = sessionId;

    _thread.Invoke([this] {
      auto instance = GetModuleHandleW(nullptr);
      WNDCLASSW windowClass{ .lpfnWndProc = WindowProcedure, .hInstance = instance, .lpszClassName = _windowClassName };
      RegisterClassW(&windowClass);

      _window = CreateWindowExW(0, _windowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
      winrt::check_bool(_window != nullptr);
      SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
      winrt::check_bool(WTSRegisterSessionNotification(_window, NOTIFY_FOR_THIS_SESSION));
      });

    auto state = Current();
    _logger.log(log_severity::information, "Watching session {}: {}, {}.", _sessionId, state.IsLocked ? "locked" : "unlocked",
      !state.IsConnected ? "disconnected" : state.IsOnConsole ? "on this PC's screen" : "through Remote Desktop");
  }

  SessionLockWatcher::~SessionLockWatcher()
  {
    _thread.Invoke([this] {
      if (!_window) return;
      WTSUnRegisterSessionNotification(_window);
      DestroyWindow(_window);
      _window = nullptr;
      });
  }

  OwnSessionState SessionLockWatcher::Current() const
  {
    OwnSessionState state;
    state.IsOnConsole = WTSGetActiveConsoleSessionId() == _sessionId;

    WTSINFOEXW* info = nullptr;
    DWORD size = 0;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, reinterpret_cast<LPWSTR*>(&info), &size) && info)
    {
      if (info->Level == 1)
      {
        auto& level = info->Data.WTSInfoExLevel1;
        state.IsLocked = level.SessionFlags == WTS_SESSIONSTATE_LOCK;
        state.IsConnected = level.SessionState != WTSDisconnected;
      }
      WTSFreeMemory(info);
    }

    return state;
  }

  LRESULT CALLBACK SessionLockWatcher::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    if (message == WM_WTSSESSION_CHANGE)
    {
      auto watcher = reinterpret_cast<SessionLockWatcher*>(GetWindowLongPtrW(window, GWLP_USERDATA));
      if (watcher) watcher->OnSessionChange(wParam);
      return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
  }

  void SessionLockWatcher::OnSessionChange(WPARAM kind)
  {
    optional<OwnSessionChange> change;
    switch (kind)
    {
    case WTS_SESSION_LOCK: change = OwnSessionChange::Locked; break;
    case WTS_SESSION_UNLOCK: change = OwnSessionChange::Unlocked; break;
    case WTS_CONSOLE_CONNECT: change = OwnSessionChange::ConsoleConnected; break;
    case WTS_CONSOLE_DISCONNECT: change = OwnSessionChange::ConsoleDisconnected; break;
    case WTS_REMOTE_CONNECT: change = OwnSessionChange::RemoteConnected; break;
    case WTS_REMOTE_DISCONNECT: change = OwnSessionChange::RemoteDisconnected; break;
    }
    if (!change) return;

    _logger.log(log_severity::information, "The session {}.", to_string(*change));
    _events.raise(Changed, this, *change);
  }
}
