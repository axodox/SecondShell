#include "pch.h"
#include "SettingsStore.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Session;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Tests
{
  TEST_CLASS(SettingsStoreTests)
  {
    path _file = temp_directory_path() / format("SecondShell.SettingsStoreTests.{}", GetCurrentProcessId()) / "settings.json";

    void Write(string_view text)
    {
      create_directories(_file.parent_path());
      ofstream{ _file, ios::binary } << text;
    }

  public:
    TEST_METHOD_CLEANUP(RemoveFiles)
    {
      remove_all(_file.parent_path());
    }

    TEST_METHOD(ChangesAreSavedAndReadBack)
    {
      Settings raised;
      SettingsStore store{ _file };
      auto subscription = store.Changed([&](SettingsStore*, const Settings& settings) { raised = settings; });
      store.Update([](Settings& settings) {
        settings.Display.Width = 2560;
        settings.Display.Height = 1440;
        settings.DefaultShell = ShellKind::Cmd;
        settings.CaptureRetention = 30;
        });
      Assert::AreEqual(2560u, raised.Display.Width, L"Changed carries the new settings");

      auto loaded = SettingsStore{ _file }.Current();
      Assert::AreEqual(1440u, loaded.Display.Height);
      Assert::IsTrue(loaded.DefaultShell == ShellKind::Cmd);
      Assert::AreEqual(30u, loaded.CaptureRetention);
      Assert::AreEqual(4u, loaded.InlineCaptureLimit, L"what was not changed keeps its default");
    }

    TEST_METHOD(AnOlderFileKeepsTheDefaultsItLacks)
    {
      Write(R"({"defaultShell": "Pwsh"})");
      auto settings = SettingsStore{ _file }.Current();
      Assert::IsTrue(settings.DefaultShell == ShellKind::Pwsh);
      Assert::AreEqual(180u, settings.SignInTimeout);
      Assert::AreEqual(1920u, settings.Display.Width);
    }

    TEST_METHOD(InvalidSettingsGiveTheDefaults)
    {
      for (auto text : { "not json", R"({"signInTimeout": 5})", R"({"defaultShell": "Bash"})" })
      {
        Write(text);
        auto settings = SettingsStore{ _file }.Current();
        Assert::AreEqual(180u, settings.SignInTimeout, to_wstring(string{ text }).c_str());
        Assert::IsTrue(settings.DefaultShell == ShellKind::PowerShell, to_wstring(string{ text }).c_str());
      }
    }
  };
}
