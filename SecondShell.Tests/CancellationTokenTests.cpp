#include "pch.h"
#include "CancellationToken.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  TEST_CLASS(CancellationTokenTests)
  {
  public:
    TEST_METHOD(CancelledIsRaisedOnce)
    {
      CancellationToken token;
      auto count = 0;
      auto subscription = token.Cancelled([&](CancellationToken*) { count++; });

      token.Cancel();
      token.Cancel();

      Assert::IsTrue(token.IsCancelled());
      Assert::AreEqual(1, count);
    }

    TEST_METHOD(WaitForTimesOutWithoutCancellation)
    {
      CancellationToken token;
      Assert::IsFalse(token.WaitFor(50ms));
      Assert::IsFalse(token.WaitFor(0ms), L"a zero timeout must not wait forever");
    }

    TEST_METHOD(WaitForReturnsWhenCancelled)
    {
      CancellationToken token;
      thread canceller([&] {
        this_thread::sleep_for(50ms);
        token.Cancel();
        });

      auto start = chrono::steady_clock::now();
      Assert::IsTrue(token.WaitFor(10s));
      Assert::IsTrue(chrono::steady_clock::now() - start < 5s);
      canceller.join();
    }
  };
}
