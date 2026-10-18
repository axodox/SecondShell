#include "pch.h"
#include "JsonConfigEditor.h"
#include "TextDiff.h"
#include "TomlConfigEditor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Integrations;
using namespace std;

namespace SecondShell::Tests
{
  namespace
  {
    const JsonEntry _entry{ { { "command", R"("C:\\SecondShell\\SecondShell.Mcp.exe")" } } };
  }

  TEST_CLASS(ConfigEditorTests)
  {
  public:
    TEST_METHOD(ANewFileGetsTheSectionAndEntry)
    {
      JsonConfigEditor editor;
      editor.SetEntry("mcpServers", "secondshell", _entry);
      Assert::AreEqual(string(R"({
  "mcpServers": {
    "secondshell": {
      "command": "C:\\SecondShell\\SecondShell.Mcp.exe"
    }
  }
}
)"), editor.Text());
    }

    TEST_METHOD(TheEntryGoesNextToTheOthersAndTheRestStays)
    {
      JsonConfigEditor editor{ R"({
    // the user's own servers
    "mcpServers": {
        "github": { "command": "gh-mcp", "args": ["--stdio"] }
    },
    "theme": "dark"
}
)" };
      editor.SetEntry("mcpServers", "secondshell", _entry);
      Assert::AreEqual(string(R"({
    // the user's own servers
    "mcpServers": {
        "github": { "command": "gh-mcp", "args": ["--stdio"] },
        "secondshell": {
            "command": "C:\\SecondShell\\SecondShell.Mcp.exe"
        }
    },
    "theme": "dark"
}
)"), editor.Text(), L"four spaces as the file has them; the comment and the other keys stay");
      Assert::IsTrue(editor.Entry("mcpServers", "secondshell").has_value());
      Assert::IsFalse(editor.Entry("servers", "secondshell").has_value());
    }

    TEST_METHOD(ASectionMissingFromAFileIsAdded)
    {
      JsonConfigEditor editor{ "{\r\n  \"theme\": \"dark\",\r\n}\r\n" };
      editor.SetEntry("servers", "secondshell", _entry);
      Assert::AreEqual(string("{\r\n  \"theme\": \"dark\",\r\n  \"servers\": {\r\n    \"secondshell\": {\r\n      \"command\": \"C:\\\\SecondShell\\\\SecondShell.Mcp.exe\"\r\n    }\r\n  }\r\n}\r\n"),
        editor.Text(), L"after the trailing comma, with the file's line breaks");
    }

    TEST_METHOD(AnEntryIsReplacedAndRemoved)
    {
      JsonConfigEditor editor{ R"({ "mcpServers": { "a": 1, "secondshell": { "command": "old" }, "b": 2 } })" };
      editor.SetEntry("mcpServers", "secondshell", { { { "command", R"("new")" } } });
      Assert::IsTrue(editor.Entry("mcpServers", "secondshell")->find("new") != string::npos);

      editor.RemoveEntry("mcpServers", "secondshell");
      Assert::AreEqual(string(R"({ "mcpServers": { "a": 1, "b": 2 } })"), editor.Text());

      editor.RemoveEntry("mcpServers", "b");
      Assert::AreEqual(string(R"({ "mcpServers": { "a": 1 } })"), editor.Text(), L"the last one, with the comma before it");

      editor.RemoveEntry("mcpServers", "a");
      Assert::AreEqual(string(R"({ "mcpServers": {} })"), editor.Text());
    }

    TEST_METHOD(BrokenFilesAreNotEdited)
    {
      for (auto text : { "[1, 2]", R"({ "mcpServers": { "a": 1 )", R"({ "mcpServers": "none" })" })
      {
        JsonConfigEditor editor{ text };
        auto wasRefused = false;
        try
        {
          editor.SetEntry("mcpServers", "secondshell", _entry);
        }
        catch (const runtime_error&)
        {
          wasRefused = true;
        }
        Assert::IsTrue(wasRefused, Axodox::Infrastructure::to_wstring(string{ text }).c_str());
      }
    }

    TEST_METHOD(TomlTablesAreAddedReplacedAndRemoved)
    {
      TomlConfigEditor editor{ "model = \"o4\"\n\n[mcp_servers.github]\ncommand = \"gh\"\n" };
      editor.SetTable("mcp_servers.secondshell", { { "command", TomlConfigEditor::Quote("C:\\SecondShell\\SecondShell.Mcp.exe") } });
      Assert::AreEqual(string("model = \"o4\"\n\n[mcp_servers.github]\ncommand = \"gh\"\n\n[mcp_servers.secondshell]\ncommand = 'C:\\SecondShell\\SecondShell.Mcp.exe'\n"), editor.Text());

      //Its subtables are part of it
      editor = TomlConfigEditor{ "[mcp_servers.secondshell]\ncommand = 'old'\n\n[mcp_servers.secondshell.env]\nA = '1'\n\n[mcp_servers.github]\ncommand = \"gh\"\n" };
      editor.SetTable("mcp_servers.secondshell", { { "command", "'new'" } });
      Assert::AreEqual(string("[mcp_servers.secondshell]\ncommand = 'new'\n\n[mcp_servers.github]\ncommand = \"gh\"\n"), editor.Text());
      Assert::AreEqual(string("command = 'new'\n\n"), *editor.Table("mcp_servers.secondshell"));

      editor.RemoveTable("mcp_servers.secondshell");
      Assert::AreEqual(string("[mcp_servers.github]\ncommand = \"gh\"\n"), editor.Text());
      Assert::IsFalse(editor.Table("mcp_servers.secondshell").has_value());
      Assert::IsFalse(TomlConfigEditor{ "[mcp_servers.secondshell2]\n" }.Table("mcp_servers.secondshell").has_value(), L"not a table of a similar name");
    }

    TEST_METHOD(TheChangeIsShownWithTheLinesAroundIt)
    {
      auto lines = ChangedLines("a\nb\nc\nd\ne\nf\n", "a\nb\nc\nX\nd\ne\nf\n", 1);
      vector<string> shown;
      for (auto& line : lines) shown.push_back(format("{}{}", line.Mark, line.Text));
      Assert::IsTrue(shown == vector<string>{ "~", " c", "+X", " d", "~" }, Axodox::Infrastructure::to_wstring(format("{} lines", shown.size())).c_str());

      auto removed = ChangedLines("a\r\nb\r\n", "a\r\n");
      Assert::IsTrue(removed.size() == 2 && removed[1].Mark == '-' && removed[1].Text == "b", L"line breaks of either kind");
    }

    TEST_METHOD(TomlStringsAreQuotedSafely)
    {
      Assert::AreEqual(string("'C:\\a b\\x.exe'"), TomlConfigEditor::Quote("C:\\a b\\x.exe"));
      Assert::AreEqual(string("\"C:\\\\O'Brien\\\\x.exe\""), TomlConfigEditor::Quote("C:\\O'Brien\\x.exe"));
    }
  };
}
