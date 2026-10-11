#include "pch.h"
#include "StaThread.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Threading;
using namespace std;

namespace SecondShell::Session
{
  //A thread message carrying a std::function<void()>* in its lParam
  const UINT _invokeMessage = WM_APP;

  StaThread::StaThread()
  {
    promise<void> ready;
    _thread = thread([this, &ready] { Run(ready); });
    ready.get_future().wait();
  }

  StaThread::~StaThread()
  {
    PostThreadMessageW(_threadId, WM_QUIT, 0, 0);
    _thread.join();
  }

  void StaThread::Invoke(const function<void()>& action)
  {
    promise<void> done;
    function<void()> task = [&] {
      try
      {
        action();
        done.set_value();
      }
      catch (...)
      {
        done.set_exception(current_exception());
      }
      };

    winrt::check_bool(PostThreadMessageW(_threadId, _invokeMessage, 0, reinterpret_cast<LPARAM>(&task)));
    done.get_future().get();
  }

  void StaThread::Run(promise<void>& ready)
  {
    set_thread_name("* STA thread");
    winrt::check_hresult(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    //Create the message queue before anyone posts to it
    MSG message;
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    _threadId = GetCurrentThreadId();
    ready.set_value();

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
      if (!message.hwnd && message.message == _invokeMessage)
      {
        (*reinterpret_cast<function<void()>*>(message.lParam))();
        continue;
      }

      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    CoUninitialize();
  }
}
