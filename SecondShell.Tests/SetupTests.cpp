#include "pch.h"
#include "ElevatedSetupLauncher.h"
#include "FakeMachineConfiguration.h"
#include "MachineSetup.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Setup;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(SetupTests)
  {
    shared_ptr<FakeMachineConfiguration> _machine = make_shared<FakeMachineConfiguration>();
    dependency_container _container;

    shared_ptr<MachineSetup> Setup()
    {
      return _container.resolve<MachineSetup>();
    }

  public:
    SetupTests()
    {
      _container.add<IMachineConfiguration>(_machine);
    }

    TEST_METHOD(ChildSessionsAreEnabledOnce)
    {
      Setup()->EnableChildSessions();
      Setup()->EnableChildSessions();
      Assert::IsTrue(Setup()->AreChildSessionsEnabled());
      Assert::AreEqual(1u, _machine->ChildSessionsEnabled, L"already enabled, it is left alone");
    }

    TEST_METHOD(TheSmoothViewIsSetAndRemovedAgain)
    {
      Assert::IsFalse(Setup()->IsSmoothSessionViewOn());

      Setup()->SetSmoothSessionView(true);
      Assert::IsTrue(_machine->Interval == optional<uint32_t>{ 15 } && _machine->IsOurs);
      Assert::IsTrue(Setup()->IsSmoothSessionViewOn());

      Setup()->SetSmoothSessionView(false);
      Assert::IsTrue(!_machine->Interval && !_machine->IsOurs, L"what SecondShell set, it removes");
    }

    TEST_METHOD(AFrameIntervalSetByOthersStays)
    {
      _machine->Interval = 10;
      Assert::IsTrue(Setup()->IsSmoothSessionViewOn(), L"10 ms is smoother still");

      Setup()->SetSmoothSessionView(true);
      Assert::IsTrue(_machine->Interval == optional<uint32_t>{ 10 } && !_machine->IsOurs, L"not changed, nor claimed");

      Setup()->SetSmoothSessionView(false);
      Assert::IsTrue(_machine->Interval == optional<uint32_t>{ 10 }, L"not removed either");

      _machine->Interval = 33;
      Assert::IsFalse(Setup()->IsSmoothSessionViewOn(), L"slower than 15 ms is not smooth");
    }

    TEST_METHOD(SetupArgumentsRoundTrip)
    {
      for (auto childSessions : { true, false })
      {
        for (auto smoothView : { optional<bool>{}, optional<bool>{ true }, optional<bool>{ false } })
        {
          auto text = SetupRequest{ childSessions, smoothView }.ToArguments();
          vector<wstring> arguments;
          for (auto part : views::split(text, L' ')) arguments.emplace_back(part.begin(), part.end());

          auto request = SetupRequest::FromArguments(arguments);
          Assert::IsTrue(request.has_value(), text.c_str());
          Assert::IsTrue(request->EnableChildSessions == childSessions && request->SmoothSessionView == smoothView, text.c_str());
        }
      }

      Assert::IsFalse(SetupRequest::FromArguments(vector<wstring>{ L"--smooth-view=on" }).has_value(), L"without --setup it is a normal start");
    }
  };
}
