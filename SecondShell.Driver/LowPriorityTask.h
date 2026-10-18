#pragma once

namespace SecondShell::Capture
{
  //A job on a thread of its own in background mode (lowest CPU, I/O and memory priority), so that long work yields to
  //the session's applications; one thread uses at most one core. Destroying the task requests a stop and waits.
  template<typename TResult>
  class LowPriorityTask
  {
  public:
    explicit LowPriorityTask(std::function<TResult(std::stop_token)> job)
    {
      std::packaged_task<TResult(std::stop_token)> task{ std::move(job) };
      _result = task.get_future();
      _thread = std::jthread{ [task = std::move(task)](std::stop_token stop) mutable {
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
        task(stop);
      } };
    }

    bool IsDone() const
    {
      return _result.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    //Waits for the job; rethrows what it threw.
    TResult Result()
    {
      return _result.get();
    }

  private:
    std::future<TResult> _result;
    std::jthread _thread;  //last, so it stops and joins first
  };
}
