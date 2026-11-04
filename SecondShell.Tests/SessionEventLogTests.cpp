#include "pch.h"
#include "SessionEventLog.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace SecondShell::Protocol;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(SessionEventLogTests)
  {
    dependency_container _container;

    static vector<SessionEventKind> Kinds(const vector<SessionEvent>& events)
    {
      vector<SessionEventKind> result;
      for (auto& event : events) result.push_back(event.Event);
      return result;
    }

    static WindowInfo Window(uint64_t handle, string title)
    {
      return { .Window = { handle }, .Title = move(title) };
    }

  public:
    TEST_METHOD(ControlChangesAreReportedOnce)
    {
      auto log = _container.resolve<SessionEventLog>();
      auto control = _container.resolve<AgentControl>();
      auto position = log->End();

      control->TakeControl();
      control->GiveBackControl();
      control->Stop();
      control->GiveBackControl();

      auto events = log->ReadFrom(position);
      Assert::IsTrue(Kinds(events) == vector{ SessionEventKind::ControlTaken, SessionEventKind::ControlReturned, SessionEventKind::AgentStopped, SessionEventKind::ControlReturned });
      Assert::IsTrue(events[0].Message.find("waitForControl") != string::npos, L"the agent learns how to wait");
      Assert::IsTrue(log->ReadFrom(position).empty(), L"each event once");
    }

    TEST_METHOD(AReaderStartingAtTheEndGetsOnlyLaterEvents)
    {
      auto log = _container.resolve<SessionEventLog>();
      log->Add(SessionEventKind::InputAvailable, "before");

      auto position = log->End();
      Assert::IsTrue(log->ReadFrom(position).empty());

      log->Add(SessionEventKind::InputAvailable, "after");
      auto events = log->ReadFrom(position);
      Assert::AreEqual(size_t(1), events.size());
      Assert::AreEqual(string("after"), events[0].Message);
    }

    TEST_METHOD(EventsHaveTheLocalTime)
    {
      auto log = _container.resolve<SessionEventLog>();
      uint64_t position = 0;
      log->Add(SessionEventKind::InputAvailable, "now");

      auto time = log->ReadFrom(position).at(0).Time;
      Assert::IsTrue(regex_match(time, regex(R"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d([+-]\d\d:\d\d|Z))")), to_wstring(time).c_str());
    }

    TEST_METHOD(LockingAndComingBackAreReported)
    {
      auto log = _container.resolve<SessionEventLog>();
      uint64_t position = log->End();

      //Locked, kept unlocked behind the lock screen, then the user signs in at the PC
      log->OnInputStatusChanged({ true, InputAvailability::Available }, { false, InputAvailability::Resuming });
      log->OnInputStatusChanged({ false, InputAvailability::Resuming }, { false, InputAvailability::Available });
      log->OnInputStatusChanged({ false, InputAvailability::Available }, { true, InputAvailability::Available });

      Assert::IsTrue(Kinds(log->ReadFrom(position)) == vector{ SessionEventKind::UserLeft, SessionEventKind::InputPaused, SessionEventKind::InputAvailable, SessionEventKind::UserReturned });

      log->OnInputStatusChanged({ true, InputAvailability::Available }, { false, InputAvailability::Unavailable });
      Assert::IsTrue(Kinds(log->ReadFrom(position)) == vector{ SessionEventKind::UserLeft, SessionEventKind::InputUnavailable });
    }

    TEST_METHOD(OldEventsAreDropped)
    {
      auto log = _container.resolve<SessionEventLog>();
      uint64_t position = 0;
      for (auto index = 0; index < 150; index++) log->Add(SessionEventKind::InputAvailable, to_string(index));

      auto events = log->ReadFrom(position);
      Assert::AreEqual(size_t(100), events.size());
      Assert::AreEqual(string("50"), events.front().Message);
      Assert::AreEqual(uint64_t(150), position);
    }

    TEST_METHOD(NewWindowsAreThoseNotSeenLastTime)
    {
      NewWindowFilter filter;
      Assert::IsTrue(filter.Update({ Window(1, "Notepad") }).empty(), L"the first look only learns what is there");

      auto added = filter.Update({ Window(1, "Notepad"), Window(2, "Save As") });
      Assert::AreEqual(size_t(1), added.size());
      Assert::AreEqual(string("Save As"), added[0].Title);

      Assert::IsTrue(filter.Update({ Window(1, "Notepad") }).empty(), L"closing is not reported");
      Assert::AreEqual(size_t(1), filter.Update({ Window(1, "Notepad"), Window(2, "Save As") }).size(), L"a window back again is new again");

      filter.Reset();
      Assert::IsTrue(filter.Update({ Window(3, "Explorer") }).empty(), L"a new session starts with a first look");
    }
  };
}
