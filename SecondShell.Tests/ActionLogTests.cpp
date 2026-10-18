#include "pch.h"
#include "ActionLog.h"
#include "CaptureActions.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "KeyboardActions.h"
#include "MouseActions.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace SecondShell::Protocol;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ActionLogTests)
  {
    ActionLog _log;

    const ActionLogEntry& Log(value_ptr<Action>&& action)
    {
      _log.Add(action, *ActionResult::Succeeded());
      _entries = _log.Sequences();
      return _entries.back().Steps.back();
    }

    vector<SequenceLogEntry> _entries;

    static string Arguments(const ActionPresentation& presentation)
    {
      string text;
      for (auto& argument : presentation.Arguments) text += format("{}{}={}", text.empty() ? "" : " ", argument.Name, argument.Value);
      return text;
    }

  public:
    TEST_METHOD(SummariesListTheArgumentsByName)
    {
      auto click = make_value<MouseClickAction>();
      click->Button = MouseButton::Right;
      click->X = 10;
      click->Y = 20;

      Assert::AreEqual(string(R"(Mouse.Click button: "Right", x: 10, y: 20)"), Log(move(click)).Summary);
    }

    TEST_METHOD(SensitiveTextIsRedacted)
    {
      auto typing = make_value<KeyboardTypeAction>();
      typing->Text = "secret";
      typing->Sensitive = true;

      auto& entry = Log(move(typing));
      Assert::AreEqual(string::npos, entry.Summary.find("secret"));
      Assert::AreEqual(string::npos, entry.Action.find("secret"));
      Assert::AreEqual(string("text=\"\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\""), Arguments(entry.Presentation), L"shown as dots, without the sensitive flag");
    }

    TEST_METHOD(LongSummariesAreShortened)
    {
      auto typing = make_value<KeyboardTypeAction>();
      typing->Text = string(300, 'a');

      auto summary = Log(move(typing)).Summary;
      Assert::IsTrue(summary.size() < 210);
      Assert::IsTrue(summary.ends_with("..."));
    }

    TEST_METHOD(ArgumentsReadAsTheUserWould)
    {
      auto wait = make_value<ControlWaitAction>();
      wait->Duration = 1500;
      Assert::AreEqual(string("duration=1.5 s"), Arguments(Log(move(wait)).Presentation));

      wait = make_value<ControlWaitAction>();
      wait->Duration = 200;
      Assert::AreEqual(string("duration=200 ms"), Arguments(Log(move(wait)).Presentation));

      wait = make_value<ControlWaitAction>();
      wait->Duration = 90000;
      Assert::AreEqual(string("duration=1 min 30 s"), Arguments(Log(move(wait)).Presentation));

      auto capture = make_value<CaptureScreenAction>();
      capture->Resolution = Size{ 1280, 720 };
      Assert::AreEqual(string("resolution=1280 \xC3\x97 720"), Arguments(Log(move(capture)).Presentation));
    }

    TEST_METHOD(ClipboardContentReadsByItsFormat)
    {
      auto write = make_value<ClipboardWriteAction>();
      write->Content.push_back({ .Format = ClipboardFormat::Text, .Data = make_value<Axodox::Json::json_string>("copied") });
      write->Content.push_back({ .Format = ClipboardFormat::Image, .Path = "C:\\a.png" });

      Assert::AreEqual(string(R"(text="copied" image="C:\a.png")"), Arguments(Log(move(write)).Presentation));
    }

    TEST_METHOD(KeysAreKeycaps)
    {
      auto tap = make_value<KeyboardTapAction>();
      tap->Key = VK_ESCAPE;
      tap->Modifiers = { KeyModifier::Ctrl, KeyModifier::Shift };

      auto& presentation = Log(move(tap)).Presentation;
      Assert::IsTrue(presentation.Keys == vector<string>{ "Ctrl", "Shift", "Esc" });
      Assert::IsTrue(presentation.Arguments.empty());
    }

    TEST_METHOD(StepsAreLoggedInTheirSequence)
    {
      _log.BeginSequence(7, "Opening the settings", 2);
      auto click = make_value<MouseClickAction>();
      _log.Start(click, 1);
      _log.Add(click, *ActionResult::Failed(ErrorCode::ActionFailed, "no"));
      _log.EndSequence(RunSequenceResponse{ .FailedStep = 1, .Error = ErrorCode::ActionFailed });

      auto sequence = _log.Sequences().back();
      Assert::AreEqual(7u, sequence.Number);
      Assert::AreEqual(string("Opening the settings"), sequence.Description);
      Assert::IsTrue(sequence.Duration.has_value());
      Assert::IsTrue(sequence.Error == ErrorCode::ActionFailed);
      Assert::AreEqual(size_t(1), sequence.Steps.size());
      Assert::AreEqual(1u, sequence.Steps[0].Step);
      Assert::IsTrue(sequence.Steps[0].Status == ActionStatus::Failed);
    }
  };
}
