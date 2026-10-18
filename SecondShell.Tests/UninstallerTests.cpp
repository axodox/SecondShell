#include "pch.h"
#include "FakeMachineConfiguration.h"
#include "Uninstaller.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Integrations;
using namespace SecondShell::Session;
using namespace SecondShell::Setup;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Tests
{
  //The uninstall against its own folders and credential: no real app, password or data folder is touched. The data
  //folder is never deleted here, as it would be the user's.
  TEST_CLASS(UninstallerTests)
  {
    path _root = temp_directory_path() / format("SecondShell.UninstallerTests.{}", GetCurrentProcessId());
    path _mcp = path{ L"C:\\SecondShell\\SecondShell.Mcp.exe" };
    shared_ptr<FakeMachineConfiguration> _machine = make_shared<FakeMachineConfiguration>();
    shared_ptr<CredentialStore> _credentials = make_shared<CredentialStore>(L"SecondShell:Tests.Uninstaller");
    dependency_container _container;

    UserFolders Folders() const
    {
      return { _root / "Profile", _root / "Roaming", _root / "Local", _root / "Programs" };
    }

  public:
    UninstallerTests()
    {
      _container.add<IMachineConfiguration>(_machine);
      _container.add<CredentialStore>(_credentials);
      _container.add<IntegrationManager>(make_shared<IntegrationManager>(IntegrationManager::Create(Folders(), _mcp), _mcp));
    }

    TEST_METHOD_CLEANUP(RemoveFiles)
    {
      remove_all(_root);
      _credentials->Delete();
    }

    TEST_METHOD(TheAppsLoseTheirEntryAndThePasswordGoesIfAsked)
    {
      create_directories(Folders().Profile / ".cursor");
      create_directories(Folders().Profile / ".gemini");
      auto integrations = _container.resolve<IntegrationManager>();
      integrations->Find("cursor")->Add();
      integrations->Find("gemini-cli")->Add();
      _credentials->Save({ L"TEST-PC\\tester", Secret{ wstring_view{ L"not-a-real-password" } } });

      auto report = _container.resolve<Uninstaller>()->Run({ .ForgetPassword = true });
      Assert::IsTrue(integrations->Find("cursor")->State() == IntegrationState::NotAdded);
      Assert::IsTrue(integrations->Find("gemini-cli")->State() == IntegrationState::NotAdded);
      Assert::IsFalse(_credentials->Load().has_value(), L"the password is gone");
      Assert::IsTrue(report.Problems.empty());
      Assert::IsTrue(ranges::find(report.Done, string{ "Removed SecondShell from Cursor." }) != report.Done.end());
      Assert::IsTrue(ranges::find(report.Done, string{ "Forgot the saved password." }) != report.Done.end());
    }

    TEST_METHOD(ThePasswordStaysUnlessAsked)
    {
      _credentials->Save({ L"TEST-PC\\tester", Secret{ wstring_view{ L"not-a-real-password" } } });
      auto report = _container.resolve<Uninstaller>()->Run({});
      Assert::IsTrue(_credentials->Load().has_value());
      Assert::IsTrue(report.Done.empty(), L"nothing to do");
    }

    TEST_METHOD(OnlyTheSmoothViewSecondShellSetIsLeftToTurnOff)
    {
      _machine->Interval = 15;
      Assert::IsFalse(_container.resolve<Uninstaller>()->Run({}).IsSmoothSessionViewLeft, L"set by someone else, it stays theirs");

      _machine->IsOurs = true;
      Assert::IsTrue(_container.resolve<Uninstaller>()->Run({}).IsSmoothSessionViewLeft);
    }
  };
}
