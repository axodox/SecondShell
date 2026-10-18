#include "pch.h"
#include "MessageWindow.h"

using namespace std;

namespace SecondShell::Desktop
{
  const wchar_t* const _className = L"SecondShell.MessageWindow";
  const UINT _invokeMessage = WM_APP;

  MessageWindow::MessageWindow() :
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
    packaged_task<void()> task{ action };
    auto result = task.get_future();
    winrt::check_bool(PostMessageW(_handle, _invokeMessage, 0, LPARAM(&task)));
    result.get();
  }

  void MessageWindow::Run()
  {
    try
    {
      WNDCLASSEXW windowClass{ .cbSize = sizeof(WNDCLASSEXW), .lpfnWndProc = &WindowProcedure, .hInstance = GetModuleHandleW(nullptr), .lpszClassName = _className };
      RegisterClassExW(&windowClass);

      auto handle = CreateWindowExW(0, _className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
      winrt::check_bool(handle);
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
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wParam, lParam);
    }
  }
}
