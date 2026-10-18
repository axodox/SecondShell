#include "pch.h"
#include "IntegrationManager.h"
#include "JsonConfigEditor.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Integrations;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Tests
{
  //The integrations against folders of their own: no real app's configuration is read or written, and none of their
  //command-line tools runs.
  TEST_CLASS(IntegrationTests)
  {
    path _root = temp_directory_path() / format("SecondShell.IntegrationTests.{}", GetCurrentProcessId());
    path _mcp = path{ L"C:\\SecondShell\\SecondShell.Mcp.exe" };

    UserFolders Folders() const
    {
      return { _root / "Profile", _root / "Roaming", _root / "Local", _root / "Programs" };
    }

    shared_ptr<AiClientIntegration> Integration(string_view id, const path& mcp) const
    {
      for (auto& integration : IntegrationManager::Create(Folders(), mcp))
      {
        if (integration->Id() == id) return integration;
      }
      Assert::Fail(L"no such integration");
    }

    static void Write(const path& file, string_view text)
    {
      create_directories(file.parent_path());
      ofstream{ file, ios::binary } << text;
    }

    static string Read(const path& file)
    {
      ifstream stream{ file, ios::binary };
      return { istreambuf_iterator<char>(stream), istreambuf_iterator<char>() };
    }

  public:
    TEST_METHOD_CLEANUP(RemoveFiles)
    {
      remove_all(_root);
    }

    TEST_METHOD(AnAppIsAddedUpdatedAndRemoved)
    {
      auto cursor = Integration("cursor", _mcp);
      Assert::IsTrue(cursor->State() == IntegrationState::NotFound);

      create_directories(Folders().Profile / ".cursor");
      Assert::IsTrue(cursor->State() == IntegrationState::NotAdded, L"its folder is there");

      cursor->Add();
      auto file = Folders().Profile / ".cursor" / "mcp.json";
      auto text = Read(file);
      Assert::IsTrue(text.find(R"("type": "stdio")") != string::npos && text.find(R"("command": "C:\\SecondShell\\SecondShell.Mcp.exe")") != string::npos, to_wstring(text).c_str());
      Assert::IsTrue(cursor->State() == IntegrationState::Added);

      //The portable build moved: the entry runs the old one
      auto moved = Integration("cursor", L"D:\\Tools\\SecondShell\\SecondShell.Mcp.exe");
      Assert::IsTrue(moved->State() == IntegrationState::Outdated);
      moved->Add();
      Assert::IsTrue(moved->State() == IntegrationState::Added);
      Assert::IsTrue(exists(path{ file } += ".bak"), L"the file before is kept");

      moved->Remove();
      Assert::IsFalse(JsonConfigEditor{ Read(file) }.Entry("mcpServers", "secondshell").has_value());
      Assert::IsTrue(moved->State() == IntegrationState::NotAdded);
    }

    TEST_METHOD(AnEntryAddedByHandIsUpdatedNotAddedTwice)
    {
      auto file = Folders().Profile / ".gemini" / "settings.json";
      Write(file, R"({ "theme": "dark", "mcpServers": { "agent-desktop": { "command": "D:\\Old\\SecondShell.Mcp.exe", "env": { "A": "1" } } } })");

      auto gemini = Integration("gemini-cli", _mcp);
      Assert::IsTrue(gemini->State() == IntegrationState::Outdated, L"found by the program it runs");

      gemini->Add();
      JsonConfigEditor editor{ Read(file) };
      Assert::IsTrue(editor.EntryNames("mcpServers") == vector<string>{ "agent-desktop" }, L"under its own name");
      auto entry = *editor.Entry("mcpServers", "agent-desktop");
      Assert::IsTrue(entry.find(R"("A": "1")") != string::npos && entry.find("C:\\\\SecondShell") != string::npos, to_wstring(entry).c_str());
      Assert::IsTrue(Read(file).starts_with(R"({ "theme": "dark")"), L"the rest stays");
    }

    TEST_METHOD(BrokenFilesAreReportedNotReplaced)
    {
      auto file = Folders().Profile / ".cursor" / "mcp.json";
      Write(file, R"({ "mcpServers": { "a": )");

      auto cursor = Integration("cursor", _mcp);
      Assert::IsTrue(cursor->State() == IntegrationState::Unreadable);

      auto wasRefused = false;
      try
      {
        cursor->Add();
      }
      catch (const runtime_error&)
      {
        wasRefused = true;
      }
      Assert::IsTrue(wasRefused);
      Assert::AreEqual(string(R"({ "mcpServers": { "a": )"), Read(file), L"left as it was");
    }

    TEST_METHOD(ClaudeDesktopFromTheStoreIsConfiguredInItsPackage)
    {
      auto package = Folders().LocalData / "Packages" / "Claude_pzs8sxrjxfjjc";
      create_directories(package);

      Integration("claude-desktop", _mcp)->Add();
      Assert::IsTrue(exists(package / "LocalCache" / "Roaming" / "Claude" / "claude_desktop_config.json"));
      Assert::IsFalse(exists(Folders().RoamingData / "Claude"), L"not where the installer build keeps it");
    }

    TEST_METHOD(LmStudioIsFoundThroughItsHomePointer)
    {
      auto home = _root / "Elsewhere" / "lmstudio";
      create_directories(home);
      Write(Folders().Profile / ".lmstudio-home-pointer", winrt::to_string(home.wstring()) + "\n");

      Integration("lm-studio", _mcp)->Add();
      Assert::IsTrue(exists(home / "mcp.json"));
    }

    TEST_METHOD(CodexGetsATomlTable)
    {
      auto codex = Integration("codex", _mcp);
      Assert::IsTrue(codex->State() == IntegrationState::NotFound);

      auto file = Folders().Profile / ".codex" / "config.toml";
      Write(file, "model = \"o4\"\n");
      Assert::IsTrue(codex->State() == IntegrationState::NotAdded);

      codex->Add();
      Assert::AreEqual(string("model = \"o4\"\n\n[mcp_servers.secondshell]\ncommand = 'C:\\SecondShell\\SecondShell.Mcp.exe'\nargs = []\n"), Read(file));
      Assert::IsTrue(codex->State() == IntegrationState::Added);
      Assert::IsTrue(Integration("codex", L"D:\\SecondShell.Mcp.exe")->State() == IntegrationState::Outdated);

      codex->Remove();
      Assert::AreEqual(string("model = \"o4\"\n"), Read(file));
    }

    TEST_METHOD(ClaudeCodeChangesGoThroughItsCommandLine)
    {
      auto claude = Integration("claude-code", _mcp);
      Write(Folders().Profile / ".local" / "bin" / "claude.exe", "");
      Write(Folders().Profile / ".claude.json", R"({ "mcpServers": { "secondshell": { "type": "stdio", "command": "D:\\Old\\SecondShell.Mcp.exe", "args": [] } } })");
      Assert::IsTrue(claude->State() == IntegrationState::Outdated);

      //Only previewed: the command line it would run, removing the old entry first
      auto change = claude->PreviewAdd();
      Assert::IsTrue(change.File.empty());
      Assert::IsTrue(change.After.find("mcp remove secondshell --scope user\n") != string::npos, to_wstring(change.After).c_str());
      Assert::IsTrue(change.After.find("mcp add --transport stdio --scope user secondshell -- C:\\SecondShell\\SecondShell.Mcp.exe") != string::npos, to_wstring(change.After).c_str());
    }
  };
}
