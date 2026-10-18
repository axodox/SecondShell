#include "pch.h"
#include "MessageWindow.h"

using namespace std;

namespace SecondShell::Desktop
{
  const wchar_t* const _className = L"SecondShell.MessageWindow";
  const UINT _invokeMessage = WM_APP;
  const UINT _postMessage = WM_APP + 1;

  MessageWindow::MessageWindow() :
    MessageReceived(_events),
    _thread([this] { Run(); })
  {
    _handle = _created.get_future().get();
  }

  MessageWindow::~MessageWindow()
  {
    PostMessageW(_handle, WM_CLOSE, 0, 0);
    _thread.join();
  }

  HWND MessageWindow::Handle() const
  {
    return _handle;
  }

  void MessageWindow::Invoke(const function<void()>& action)
  {
    //On the window's thread already, e.g. from a message handler: waiting for it would never end
    if (GetCurrentThreadId() == _threadId)
    {
      action();
      return;
    }

    packaged_task<void()> task{ action };
    auto result = task.get_future();
    winrt::check_bool(PostMessageW(_handle, _invokeMessage, 0, LPARAM(&task)));
    result.get();
  }

  void MessageWindow::Post(function<void()> action)
  {
    auto posted = make_unique<function<void()>>(move(action));
    winrt::check_bool(PostMessageW(_handle, _postMessage, 0, LPARAM(posted.get())));
    posted.release();
  }

  void MessageWindow::Run()
  {
    try
    {
      WNDCLASSEXW windowClass{ .cbSize = sizeof(WNDCLASSEXW), .lpfnWndProc = &WindowProcedure, .hInstance = GetModuleHandleW(nullptr), .lpszClassName = _className };
      RegisterClassExW(&windowClass);

      auto handle = CreateWindowExW(0, _className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
      winrt::check_bool(handle);
      SetWindowLongPtrW(handle, GWLP_USERDATA, LONG_PTR(this));
      _threadId = GetCurrentThreadId();
      _created.set_value(handle);
    }
    catch (...)
    {
      _created.set_exception(current_exception());
      return;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }

  LRESULT CALLBACK MessageWindow::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case _invokeMessage:
      (*reinterpret_cast<packaged_task<void()>*>(lParam))();
      return 0;
    case _postMessage:
    {
      unique_ptr<function<void()>> action{ reinterpret_cast<function<void()>*>(lParam) };
      try
      {
        (*action)();
      }
      catch (const exception& error)
      {
        _logger.log(Axodox::Infrastructure::log_severity::error, "A posted action failed: {}", error.what());
      }
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      //Nothing to tell before the window knows its owner, during its creation
      if (auto owner = reinterpret_cast<MessageWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA)))
      {
        owner->_events.raise(owner->MessageReceived, owner, message);
      }
      return DefWindowProcW(window, message, wParam, lParam);
    }
  }
}
