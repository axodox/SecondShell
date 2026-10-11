#pragma once

namespace SecondShell::Session
{
  //A thread in a single-threaded apartment with a message loop, for COM objects that need one (the RDP control).
  class StaThread
  {
  public:
    StaThread();
    ~StaThread();

    StaThread(const StaThread&) = delete;
    StaThread& operator=(const StaThread&) = delete;

    //Runs the action on the thread and waits for it; rethrows its exception.
    void Invoke(const std::function<void()>& action);

  private:
    std::thread _thread;
    DWORD _threadId = 0;

    void Run(std::promise<void>& ready);
  };
}
