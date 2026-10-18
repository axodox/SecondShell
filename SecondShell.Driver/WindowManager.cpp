#include "pch.h"
#include "WindowManager.h"

using namespace SecondShell::Actions;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Desktop
{
  //Long enough for an application which is busy for a moment, short enough not to stall a sequence
  const milliseconds _changeTimeout{ 2000 };
  const milliseconds _pollInterval{ 10 };

  namespace
  {
    bool WaitUntil(const function<bool()>& isDone)
    {
      auto deadline = steady_clock::now() + _changeTimeout;
      while (!isDone())
      {
        if (steady_clock::now() > deadline) return false;
        this_thread::sleep_for(_pollInterval);
      }
      return true;
    }

    bool IsCloaked(HWND handle)
    {
      DWORD cloaked = 0;
      return SUCCEEDED(DwmGetWindowAttribute(handle, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked;
    }

    //As the taskbar and Alt+Tab list them: visible, not cloaked (e.g. a suspended app), and neither a tool window nor
    //an owned popup, unless the window asks to be listed.
    bool IsListed(HWND handle)
    {
      if (handle == GetShellWindow() || !IsWindowVisible(handle) || IsCloaked(handle)) return false;

      auto style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
      if (style & WS_EX_APPWINDOW) return true;
      return !(style & WS_EX_TOOLWINDOW) && !GetWindow(handle, GW_OWNER);
    }

    RECT FrameBounds(HWND handle)
    {
      RECT bounds;
      if (FAILED(DwmGetWindowAttribute(handle, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds)))) GetWindowRect(handle, &bounds);
      return bounds;
    }

    WindowState StateOf(HWND handle)
    {
      if (IsIconic(handle)) return WindowState::Minimized;
      if (IsZoomed(handle)) return WindowState::Maximized;
      return WindowState::Normal;
    }

    string ProcessName(DWORD processId)
    {
      winrt::handle process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };
      if (!process) return {};

      wchar_t path[MAX_PATH];
      DWORD size = MAX_PATH;
      if (!QueryFullProcessImageNameW(process.get(), 0, path, &size)) return {};
      return filesystem::path(path).filename().string();
    }

    string Title(HWND handle)
    {
      wstring title(size_t(GetWindowTextLengthW(handle)) + 1, L'\0');
      title.resize(GetWindowTextW(handle, title.data(), int(title.size())));
      return winrt::to_string(title);
    }

    WindowInfo Describe(HWND handle)
    {
      WindowInfo info;
      info.Window = { uint64_t(handle) };
      info.Title = Title(handle);
      DWORD processId = 0;
      GetWindowThreadProcessId(handle, &processId);
      info.ProcessId = processId;
      info.ProcessName = ProcessName(processId);
      info.State = StateOf(handle);
      info.IsForeground = handle == GetForegroundWindow();

      //A minimized window's frame is off the screen; its restored position is more useful
      RECT bounds = FrameBounds(handle);
      if (info.State == WindowState::Minimized)
      {
        WINDOWPLACEMENT placement{ .length = sizeof(WINDOWPLACEMENT) };
        if (GetWindowPlacement(handle, &placement)) bounds = placement.rcNormalPosition;
      }

      info.X = bounds.left;
      info.Y = bounds.top;
      info.Width = uint32_t(max(bounds.right - bounds.left, LONG(0)));
      info.Height = uint32_t(max(bounds.bottom - bounds.top, LONG(0)));
      return info;
    }
  }

  vector<WindowInfo> WindowManager::Enumerate() const
  {
    //In Z order, the topmost first
    vector<WindowInfo> windows;
    EnumWindows([](HWND handle, LPARAM context) {
      if (IsListed(handle)) reinterpret_cast<vector<WindowInfo>*>(context)->push_back(Describe(handle));
      return TRUE;
      }, LPARAM(&windows));
    return windows;
  }

  WindowInfo WindowManager::Get(WindowId window) const
  {
    return Describe(Handle(window));
  }

  optional<WindowId> WindowManager::Foreground() const
  {
    auto handle = GetForegroundWindow();
    if (!handle) return nullopt;
    return WindowId{ uint64_t(handle) };
  }

  bool WindowManager::Focus(WindowId window)
  {
    auto handle = Handle(window);
    if (IsIconic(handle)) SetState(window, WindowState::Normal);

    //Windows lets a process take the foreground only in some cases, e.g. when it sent the last input. The input injected
    //for the agent does not count, but a mouse move by nothing sent here does, and changes nothing.
    if (!SetForegroundWindow(handle))
    {
      INPUT input{ .type = INPUT_MOUSE, .mi = { .dwFlags = MOUSEEVENTF_MOVE } };
      SendInput(1, &input, sizeof(INPUT));
      SetForegroundWindow(handle);
    }

    return WaitUntil([handle] { return GetForegroundWindow() == handle; });
  }

  void WindowManager::Move(WindowId window, int32_t x, int32_t y)
  {
    auto handle = Handle(window);
    if (StateOf(handle) != WindowState::Normal) SetState(window, WindowState::Normal);

    auto frame = FrameBounds(handle);
    SetBounds(handle, { x, y, x + frame.right - frame.left, y + frame.bottom - frame.top });
  }

  void WindowManager::Resize(WindowId window, uint32_t width, uint32_t height)
  {
    auto handle = Handle(window);
    if (StateOf(handle) != WindowState::Normal) SetState(window, WindowState::Normal);

    auto frame = FrameBounds(handle);
    SetBounds(handle, { frame.left, frame.top, frame.left + LONG(width), frame.top + LONG(height) });
  }

  bool WindowManager::SetState(WindowId window, WindowState state)
  {
    auto handle = Handle(window);
    switch (state)
    {
    case WindowState::Minimized:
      ShowWindowAsync(handle, SW_MINIMIZE);
      break;
    case WindowState::Maximized:
      ShowWindowAsync(handle, SW_MAXIMIZE);
      break;
    default:
      ShowWindowAsync(handle, SW_RESTORE);
      break;
    }

    return WaitUntil([handle, state] { return StateOf(handle) == state; });
  }

  void WindowManager::Close(WindowId window)
  {
    PostMessageW(Handle(window), WM_SYSCOMMAND, SC_CLOSE, 0);
  }

  HWND WindowManager::Handle(WindowId window) const
  {
    auto handle = reinterpret_cast<HWND>(window.Handle);
    if (!IsWindow(handle) || !IsWindowVisible(handle)) throw invalid_argument(format("There is no window {}; Window.Enumerate lists them.", window.to_string()));
    return handle;
  }

  void WindowManager::SetBounds(HWND handle, const RECT& frame)
  {
    //The window's own rectangle includes the invisible resize borders around the frame
    auto current = FrameBounds(handle);
    RECT window;
    GetWindowRect(handle, &window);

    auto x = frame.left - (current.left - window.left);
    auto y = frame.top - (current.top - window.top);
    auto width = (frame.right - frame.left) + (window.right - window.left) - (current.right - current.left);
    auto height = (frame.bottom - frame.top) + (window.bottom - window.top) - (current.bottom - current.top);
    SetWindowPos(handle, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);

    //Applications may limit their size; the agent sees the result in Window.Enumerate
    WaitUntil([handle, frame] {
      auto bounds = FrameBounds(handle);
      return bounds.left == frame.left && bounds.top == frame.top && bounds.right == frame.right && bounds.bottom == frame.bottom;
      });
  }
}
