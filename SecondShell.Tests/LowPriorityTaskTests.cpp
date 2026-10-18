#include "pch.h"
#include "LowPriorityTask.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Tests
{
  TEST_CLASS(LowPriorityTaskTests)
  {
  public:
    TEST_METHOD(TheJobRunsInTheBackground)
    {
      auto caller = this_thread::get_id();
      LowPriorityTask<bool> task{ [caller](stop_token) { return this_thread::get_id() != caller; } };

      while (!task.IsDone()) this_thread::sleep_for(1ms);
      Assert::IsTrue(task.Result(), L"on a thread of its own");
    }

    TEST_METHOD(FailuresReachTheCaller)
    {
      LowPriorityTask<int> task{ [](stop_token) -> int { throw runtime_error("failed"); } };
      Assert::ExpectException<runtime_error>([&] { task.Result(); });
    }

    TEST_METHOD(DestroyingTheTaskStopsTheJob)
    {
      auto started = steady_clock::now();
      {
        LowPriorityTask<int> task{ [](stop_token stop) {
          while (!stop.stop_requested()) this_thread::sleep_for(1ms);
          return 0;
          } };
      }
      Assert::IsTrue(steady_clock::now() - started < 5s);
    }
  };
}
