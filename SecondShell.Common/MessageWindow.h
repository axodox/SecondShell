#pragma once
#include "CommonApi.h"

namespace SecondShell::Desktop
{
  //A message-only window on a thread of its own, for the APIs which need a window, such as the clipboard's.
  class SECONDSHELL_COMMON_API MessageWindow
  {
    inline static const Axodox::Infrastructure::logger _logger{ "MessageWindow" };
    Axodox::Infrastructure::event_owner _events;

  public:
    MessageWindow();
    ~MessageWindow();

    MessageWindow(const MessageWindow&) = delete;
    MessageWindow& operator=(const MessageWindow&) = delete;

    HWND Handle() const;

    //Runs the action on the window's thread and returns once it is done; rethrows what the action throws.
    void Invoke(const std::function<void()>& action);

    //Runs the action later on the window's thread, in the order posted; what it throws is logged.
    void Post(std::function<void()> action);

    //A message the window does not handle itself, e.g. WM_CLIPBOARDUPDATE; raised on the window's thread.
    Axodox::Infrastructure::event_publisher<MessageWindow*, UINT> MessageReceived;

  private:
    HWND _handle = nullptr;
    DWORD _threadId = 0;
    std::promise<HWND> _created;
    std::thread _thread;

    void Run();
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
  };
}
