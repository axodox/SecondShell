#include "pch.h"
#include "LockScreenPolicy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
  template<> inline wstring ToString<LockScreenAction>(const LockScreenAction& action)
  {
    switch (action)
    {
    case LockScreenAction::Connect: return L"Connect";
    case LockScreenAction::Disconnect: return L"Disconnect";
    default: return L"None";
    }
  }
}

namespace SecondShell::Tests
{
  //The sequences of the live tests (Findings.md 2.17), as WTS reports them.
  TEST_CLASS(LockScreenPolicyTests)
  {
    using clock = LockScreenPolicy::clock;

    const OwnSessionState _atThePc{ .IsLocked = false, .IsConnected = true, .IsOnConsole = true };
    const OwnSessionState _lockedAtThePc{ .IsLocked = true, .IsConnected = true, .IsOnConsole = true };
    const OwnSessionState _overLoopback{ .IsLocked = false, .IsConnected = true, .IsOnConsole = false };
    const OwnSessionState _disconnected{ .IsLocked = true, .IsConnected = false, .IsOnConsole = false };
    const OwnSessionState _lockedUnderRemoteDesktop{ .IsLocked = true, .IsConnected = true, .IsOnConsole = false };

    clock::time_point _now = clock::time_point{} + hours(1);

    LockScreenPolicy Active()
    {
      LockScreenPolicy policy;
      policy.SetActive(true, _now, _atThePc);
      return policy;
    }

    //Locked at the PC, then connected after the delay
    LockScreenPolicy Connected()
    {
      auto policy = Active();
      policy.OnLocked(_now);
      _now += LockScreenPolicy::LockDelay;
      Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now, _lockedAtThePc));

      //Its own connection: the session leaves the PC's screen, and unlocks there
      policy.OnLocked(_now);
      policy.OnUnlocked(_overLoopback);
      policy.OnLoopbackConnected();
      return policy;
    }

  public:
    TEST_METHOD(ALockConnectsAfterTheDelay)
    {
      auto policy = Active();
      policy.OnLocked(_now);
      Assert::IsTrue(policy.NextCheck() == _now + LockScreenPolicy::LockDelay);
      Assert::AreEqual(LockScreenAction::None, policy.OnCheck(_now + seconds(1), _lockedAtThePc), L"not before the delay");
      Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedAtThePc));
      Assert::IsTrue(policy.IsLoopbackActive());
    }

    TEST_METHOD(NothingHappensWhileOff)
    {
      LockScreenPolicy policy;
      policy.OnLocked(_now);
      Assert::IsFalse(policy.NextCheck().has_value());
      Assert::AreEqual(LockScreenAction::None, policy.OnCheck(_now + hours(1), _lockedAtThePc));
    }

    TEST_METHOD(AnUnlockWithinTheDelayCancels)
    {
      auto policy = Active();
      policy.OnLocked(_now);
      policy.OnUnlocked(_atThePc);
      Assert::IsFalse(policy.NextCheck().has_value());
      Assert::AreEqual(LockScreenAction::None, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _atThePc));
    }

    TEST_METHOD(UnlockedAgainWhenTheCheckIsDueConnectsNothing)
    {
      auto policy = Active();
      policy.OnLocked(_now);
      Assert::AreEqual(LockScreenAction::None, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _atThePc));
    }

    TEST_METHOD(ADisconnectConnectsToo)
    {
      //tsdiscon: the session leaves the PC's screen, locked
      auto policy = Active();
      policy.OnLocked(_now);
      Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _disconnected));
    }

    TEST_METHOD(ItsOwnConnectionArmsNothing)
    {
      auto policy = Connected();
      Assert::IsFalse(policy.NextCheck().has_value(), L"its own lock and unlock change nothing");
      Assert::IsTrue(policy.IsLoopbackActive());
    }

    TEST_METHOD(SigningInAtThePcEndsItAndTheBriefLockIsIgnored)
    {
      auto policy = Connected();

      //The user signs in at the PC: the session comes back to the screen, briefly locked, and the connection ends
      Assert::AreEqual(LockScreenAction::Disconnect, policy.OnConsoleConnected());
      policy.OnLocked(_now);
      policy.OnLoopbackDisconnected(_now, _lockedAtThePc);
      Assert::IsFalse(policy.NextCheck().has_value(), L"no connection after the user's own sign-in");
      Assert::IsFalse(policy.IsLoopbackActive());

      //Unlocked at the PC, the next lock counts again
      policy.OnUnlocked(_atThePc);
      policy.OnLocked(_now);
      Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedAtThePc));
    }

    TEST_METHOD(TheConnectionEndingBeforeTheConsoleConnectsIsNotRetriedOnceItDoes)
    {
      //The connection may report its end before Windows reports the sign-in at the PC
      auto policy = Connected();
      policy.OnLoopbackDisconnected(_now, _disconnected);
      Assert::IsTrue(policy.NextCheck().has_value());

      policy.OnConsoleConnected();
      Assert::IsFalse(policy.NextCheck().has_value());
      Assert::AreEqual(LockScreenAction::None, policy.OnCheck(_now + LockScreenPolicy::RetryDelay, _lockedAtThePc));
    }

    TEST_METHOD(AConnectionLostWhileAwayIsRetriedAFewTimes)
    {
      auto policy = Connected();
      for (uint32_t retry = 0; retry < LockScreenPolicy::MaxRetries; retry++)
      {
        policy.OnLoopbackDisconnected(_now, _disconnected);
        Assert::IsTrue(policy.NextCheck() == _now + LockScreenPolicy::RetryDelay);
        _now += LockScreenPolicy::RetryDelay;
        Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now, _disconnected));
      }

      policy.OnLoopbackDisconnected(_now, _disconnected);
      Assert::IsFalse(policy.NextCheck().has_value(), L"gives up after the last retry");
    }

    TEST_METHOD(ASuccessfulConnectionResetsTheRetries)
    {
      auto policy = Connected();
      policy.OnLoopbackDisconnected(_now, _disconnected);
      _now += LockScreenPolicy::RetryDelay;
      Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now, _disconnected));
      policy.OnLoopbackConnected();

      for (uint32_t retry = 0; retry < LockScreenPolicy::MaxRetries; retry++)
      {
        policy.OnLoopbackDisconnected(_now, _disconnected);
        Assert::IsTrue(policy.NextCheck().has_value(), L"the full number of retries again");
        _now += LockScreenPolicy::RetryDelay;
        policy.OnCheck(_now, _disconnected);
      }
    }

    TEST_METHOD(NoRetryWhenItCannotWork)
    {
      auto policy = Active();
      policy.OnLocked(_now);
      policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedAtThePc);
      policy.OnConnectFailed(_now, false);
      Assert::IsFalse(policy.NextCheck().has_value());
      Assert::IsFalse(policy.IsLoopbackActive());
    }

    TEST_METHOD(AFailedStartIsRetried)
    {
      auto policy = Active();
      policy.OnLocked(_now);
      policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedAtThePc);
      policy.OnConnectFailed(_now, true);
      Assert::IsTrue(policy.NextCheck() == _now + LockScreenPolicy::RetryDelay);
    }

    TEST_METHOD(TurningOffEndsTheConnection)
    {
      auto policy = Connected();
      Assert::AreEqual(LockScreenAction::Disconnect, policy.SetActive(false, _now, _overLoopback));
      Assert::IsFalse(policy.IsLoopbackActive());

      policy.OnLoopbackDisconnected(_now, _disconnected);
      Assert::IsFalse(policy.NextCheck().has_value(), L"no retry once off");
    }

    TEST_METHOD(TurnedOnWhileAwayConnects)
    {
      LockScreenPolicy policy;
      policy.SetActive(true, _now, _lockedAtThePc);
      Assert::AreEqual(LockScreenAction::Connect, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedAtThePc));
    }

    TEST_METHOD(ResumingFromTheLockUntilTheConnectionEnds)
    {
      auto policy = Active();
      Assert::IsFalse(policy.IsResuming());
      policy.OnLocked(_now);
      Assert::IsTrue(policy.IsResuming(), L"during the delay");
      policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedAtThePc);
      Assert::IsTrue(policy.IsResuming(), L"while connecting");
      policy.OnConnectFailed(_now, false);
      Assert::IsFalse(policy.IsResuming(), L"not when it cannot work");

      LockScreenPolicy off;
      off.OnLocked(_now);
      Assert::IsFalse(off.IsResuming(), L"never while off");
    }

    TEST_METHOD(ASessionLockedUnderSomeonesRemoteDesktopIsLeftAlone)
    {
      //Connecting would throw the user's own Remote Desktop client out
      auto policy = Active();
      policy.OnLocked(_now);
      Assert::AreEqual(LockScreenAction::None, policy.OnCheck(_now + LockScreenPolicy::LockDelay, _lockedUnderRemoteDesktop));
    }
  };
}
