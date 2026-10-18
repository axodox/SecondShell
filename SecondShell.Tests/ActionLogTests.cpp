#include "pch.h"
#include "ActionLog.h"
#include "KeyboardActions.h"
#include "MouseActions.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ActionLogTests)
  {
    ActionLog _log;

    string Summary(value_ptr<Action>&& action)
    {
      _log.Add(action, *ActionResult::Succeeded());
      return _log.Entries().back().Summary;
    }

  public:
    TEST_METHOD(SummariesListTheArgumentsByName)
    {
      auto click = make_value<MouseClickAction>();
      click->Button = MouseButton::Right;
      click->X = 10;
      click->Y = 20;

      Assert::AreEqual(string(R"(Mouse.Click button: "Right", x: 10, y: 20)"), Summary(move(click)));
    }

    TEST_METHOD(SensitiveTextIsRedacted)
    {
      auto typing = make_value<KeyboardTypeAction>();
      typing->Text = "secret";
      typing->Sensitive = true;

      auto summary = Summary(move(typing));
      Assert::AreEqual(string::npos, summary.find("secret"));
      Assert::AreEqual(string::npos, _log.Entries().back().Action.find("secret"));
    }

    TEST_METHOD(LongSummariesAreShortened)
    {
      auto typing = make_value<KeyboardTypeAction>();
      typing->Text = string(300, 'a');

      auto summary = Summary(move(typing));
      Assert::IsTrue(summary.size() < 210);
      Assert::IsTrue(summary.ends_with("..."));
    }
  };
}
