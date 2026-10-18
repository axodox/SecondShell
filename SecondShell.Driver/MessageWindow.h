#pragma once

namespace SecondShell::Desktop
{
  //A message-only window on a thread of its own, for the APIs which need a window, such as the clipboard's.
  class MessageWindow
  {
  public:
    MessageWindow();
    ~MessageWindow();

    MessageWindow(const MessageWindow&) = delete;
    MessageWindow& operator=(const MessageWindow&) = delete;

    HWND Handle() const;

    //Runs the action on the window's thread and returns once it is done; rethrows what the action throws.
    void Invoke(const std::function<void()>& action);

  private:
    HWND _handle = nullptr;
    std::promise<HWND> _created;
    std::thread _thread;

    void Run();
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
  };
}
