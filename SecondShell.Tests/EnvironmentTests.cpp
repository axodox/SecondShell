#include "pch.h"
#include "AppEnvironment.h"
#include "PipeNames.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Environment;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(EnvironmentTests)
  {
  public:
    TEST_METHOD(PipeNamesAreDistinctAndPerUser)
    {
      auto driver = PipeNames::Driver();
      auto mcp = PipeNames::Mcp();

      Assert::AreNotEqual(driver, mcp);
      Assert::IsTrue(driver.starts_with("SecondShell.S-1-"), L"the name should contain the user's SID");
      Assert::IsTrue(driver.find('\\') == string::npos, L"a pipe name must not contain a backslash");
    }

    TEST_METHOD(ExecutablesAreNextToTheLibrary)
    {
      AppEnvironment environment;
      if (environment.IsPackaged()) return;

      auto folder = environment.HostExecutable().parent_path();
      Assert::IsTrue(filesystem::exists(folder / "SecondShell.Common.dll"));
      Assert::IsTrue(environment.DriverExecutable().parent_path() == folder);
      Assert::IsTrue(environment.McpExecutable().parent_path() == folder);
    }

    TEST_METHOD(FoldersAreUnderTheDataFolder)
    {
      AppEnvironment environment;

      Assert::AreEqual(wstring(L"SecondShell"), environment.DataFolder().filename().wstring());
      Assert::IsTrue(environment.CaptureFolder("session-1") == environment.DataFolder() / "Captures" / "session-1");
      Assert::IsTrue(environment.LogFolder().parent_path() == environment.DataFolder());
    }
  };
}
