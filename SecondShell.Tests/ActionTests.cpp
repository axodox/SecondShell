#include "pch.h"
#include "ActionCatalog.h"
#include "CaptureActions.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "JsonSnapshot.h"
#include "KeyboardActions.h"
#include "MouseActions.h"
#include "ProcessActions.h"
#include "TouchActions.h"
#include "WindowActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  TEST_CLASS(ActionTests)
  {
    //The examples of Design.md (Input simulation), with their placeholders filled in
    inline static const vector<pair<ActionType, string>> _examples = {
      { ActionType::ControlWait, R"({"action": "Control.Wait", "duration": 500})" },
      { ActionType::ControlJoin, R"({"action": "Control.Join"})" },
      { ActionType::ControlReset, R"({"action": "Control.Reset"})" },
      { ActionType::ProcessRun, R"({"action": "Process.Run", "command": "notepad.exe", "shell": "None", "args": ["a.txt"], "workingDirectory": "C:\\Temp", "async": true, "timeout": 5000})" },
      { ActionType::ProcessStop, R"({"action": "Process.Stop", "pid": 1234, "timeout": 5000})" },
      { ActionType::ProcessKill, R"({"action": "Process.Kill", "pid": 1234, "tree": true})" },
      { ActionType::ProcessWaitForProcess, R"({"action": "Process.WaitForProcess", "pid": 1234, "timeout": 5000})" },
      { ActionType::ProcessEnumerate, R"({"action": "Process.Enumerate"})" },
      { ActionType::CaptureScreen, R"({"action": "Capture.Screen", "tag": "after", "output": "file", "path": "C:\\Temp\\a.jpg", "format": "jpeg", "resolution": {"width": 1280, "height": 720}})" },
      { ActionType::CaptureWindow, R"({"action": "Capture.Window", "window": "0x0001A2B4"})" },
      { ActionType::CaptureRegion, R"({"action": "Capture.Region", "x": 10, "y": 20, "width": 300, "height": 200})" },
      { ActionType::CaptureWaitForText, R"({"action": "Capture.WaitForText", "text": "Saved", "interval": 500, "timeout": 10000})" },
      { ActionType::CaptureWaitForStable, R"({"action": "Capture.WaitForStable", "stable": 500, "window": "0x0001A2B4", "interval": 100, "timeout": 10000})" },
      { ActionType::MouseMove, R"({"action": "Mouse.Move", "x": 100, "y": 200, "duration": 300})" },
      { ActionType::MouseClick, R"({"action": "Mouse.Click", "button": "Right", "x": 100, "y": 200})" },
      { ActionType::MouseDoubleClick, R"({"action": "Mouse.DoubleClick", "button": "Left", "x": 100, "y": 200})" },
      { ActionType::MouseWheel, R"({"action": "Mouse.Wheel", "delta": -2.5, "axis": "Horizontal", "x": 100, "y": 200})" },
      { ActionType::MousePress, R"({"action": "Mouse.Press", "button": "Middle"})" },
      { ActionType::MouseRelease, R"({"action": "Mouse.Release", "button": "Middle"})" },
      { ActionType::MouseReset, R"({"action": "Mouse.Reset"})" },
      { ActionType::TouchPress, R"({"action": "Touch.Press", "id": 1, "x": 10, "y": 20})" },
      { ActionType::TouchRelease, R"({"action": "Touch.Release", "id": 1})" },
      { ActionType::TouchTap, R"({"action": "Touch.Tap", "id": 2, "x": 10, "y": 20})" },
      { ActionType::TouchMove, R"({"action": "Touch.Move", "id": 1, "x": 30, "y": 40, "duration": 200})" },
      { ActionType::TouchReset, R"({"action": "Touch.Reset"})" },
      { ActionType::KeyboardType, R"({"action": "Keyboard.Type", "text": "Hello\nWorld", "pause": 20, "sensitive": true})" },
      { ActionType::KeyboardTap, R"({"action": "Keyboard.Tap", "key": "S", "modifiers": ["Ctrl"]})" },
      { ActionType::KeyboardPress, R"({"action": "Keyboard.Press", "key": "LWin"})" },
      { ActionType::KeyboardRelease, R"({"action": "Keyboard.Release", "key": "LWin"})" },
      { ActionType::KeyboardReset, R"({"action": "Keyboard.Reset"})" },
      { ActionType::WindowEnumerate, R"({"action": "Window.Enumerate"})" },
      { ActionType::WindowFocus, R"({"action": "Window.Focus", "window": "0x0001A2B4"})" },
      { ActionType::WindowMove, R"({"action": "Window.Move", "window": "0x0001A2B4", "x": 0, "y": 0})" },
      { ActionType::WindowResize, R"({"action": "Window.Resize", "window": "0x0001A2B4", "width": 800, "height": 600})" },
      { ActionType::WindowMinimize, R"({"action": "Window.Minimize", "window": "0x0001A2B4"})" },
      { ActionType::WindowMaximize, R"({"action": "Window.Maximize", "window": "0x0001A2B4"})" },
      { ActionType::WindowRestore, R"({"action": "Window.Restore", "window": "0x0001A2B4"})" },
      { ActionType::WindowClose, R"({"action": "Window.Close", "window": "0x0001A2B4"})" },
      { ActionType::ClipboardRead, R"({"action": "Clipboard.Read", "format": "text"})" },
      { ActionType::ClipboardWrite, R"({"action": "Clipboard.Write", "content": [{"format": "text", "data": "hello"}, {"format": "html", "data": "<b>hello</b>"}]})" }
    };

    static value_ptr<Action> Parse(string_view text)
    {
      auto json = json_value::from_string(text);
      Assert::IsTrue(bool(json), L"the test JSON must be valid");

      auto result = ActionCatalog::Parse(*json);
      if (!result) Assert::Fail(to_wstring(result.error()).c_str());
      return *result;
    }

    template<typename TAction>
    static TAction ParseAs(string_view text)
    {
      return static_cast<const TAction&>(*Parse(text));
    }

    static string ParseError(string_view text)
    {
      auto json = json_value::from_string(text);
      auto result = ActionCatalog::Parse(*json);
      Assert::IsFalse(bool(result), to_wstring(format("{} should be rejected", text)).c_str());
      return result.error();
    }

    static void AssertContains(const string& text, string_view part)
    {
      Assert::IsTrue(text.find(part) != string::npos, to_wstring(format("'{}' should contain '{}'", text, part)).c_str());
    }

  public:
    TEST_METHOD(DocumentedExamplesParse)
    {
      for (auto& [type, text] : _examples)
      {
        auto action = Parse(text);
        Assert::AreEqual(to_string(type), to_string(action->Type()), to_wstring(text).c_str());
      }
    }

    TEST_METHOD(ExamplesCoverEveryAction)
    {
      set<string> examples;
      for (auto& [type, text] : _examples) examples.insert(Parse(text)->Name());

      for (auto& definition : ActionCatalog::Actions())
      {
        Assert::IsTrue(examples.contains(definition.Name), to_wstring(format("{} has no example", definition.Name)).c_str());
      }
      Assert::AreEqual(ActionCatalog::Actions().size(), examples.size());
    }

    TEST_METHOD(PropertiesAreRead)
    {
      auto run = ParseAs<ProcessRunAction>(_examples[3].second);
      Assert::AreEqual(string("notepad.exe"), run.Command);
      Assert::IsTrue(run.Shell == ShellKind::None);
      Assert::AreEqual(size_t(1), run.Args.size());
      Assert::AreEqual(string("C:\\Temp"), *run.WorkingDirectory);
      Assert::IsTrue(*run.Async);
      Assert::IsFalse(run.IsAsync(), L"an async Process.Run returns the process ID at once, and Control.Join does not wait for it");
      Assert::IsTrue(run.WaitTimeout() == 5000ms);

      auto capture = ParseAs<CaptureScreenAction>(_examples[8].second);
      Assert::AreEqual(string("after"), *capture.Tag);
      Assert::IsTrue(capture.Output == CaptureOutput::File, L"enum values are case-insensitive");
      Assert::IsTrue(capture.Format == ImageFormat::Jpeg);
      Assert::AreEqual(1280u, capture.Resolution->Width);

      auto window = ParseAs<CaptureWindowAction>(_examples[9].second);
      Assert::AreEqual(uint64_t(0x1A2B4), window.Window->Handle);

      auto wheel = ParseAs<MouseWheelAction>(_examples[16].second);
      Assert::AreEqual(-2.5f, wheel.Delta);
      Assert::IsTrue(wheel.Axis == WheelAxis::Horizontal);

      auto tap = ParseAs<KeyboardTapAction>(_examples[26].second);
      Assert::AreEqual(uint16_t('S'), tap.Key);
      Assert::AreEqual(size_t(1), tap.Modifiers.size());
      Assert::IsTrue(tap.Modifiers[0] == KeyModifier::Ctrl);

      auto type = ParseAs<KeyboardTypeAction>(_examples[25].second);
      Assert::AreEqual(string("Hello\nWorld"), type.Text);

      auto write = ParseAs<ClipboardWriteAction>(_examples[39].second);
      Assert::AreEqual(size_t(2), write.Content.size());
      Assert::IsTrue(write.Content[1].Format == ClipboardFormat::Html);
      Assert::AreEqual(string("<b>hello</b>"), write.Content[1].Data->as<json_string>().value);
    }

    TEST_METHOD(DefaultsApply)
    {
      auto click = ParseAs<MouseClickAction>(R"({"action": "Mouse.Click"})");
      Assert::IsTrue(click.Button == MouseButton::Left);
      Assert::IsFalse(click.X.has_value());
      Assert::IsFalse(click.IsAsync());

      auto wait = ParseAs<ProcessWaitForProcessAction>(R"({"action": "Process.WaitForProcess", "pid": 5})");
      Assert::IsFalse(wait.WaitTimeout().has_value(), L"the driver applies Action::DefaultWaitTimeout");
    }

    TEST_METHOD(ActionNamesAreCaseInsensitive)
    {
      Assert::IsTrue(Parse(R"({"action": "mouse.click"})")->Type() == ActionType::MouseClick);
    }

    TEST_METHOD(KeysAcceptNamesAliasesAndCodes)
    {
      Assert::AreEqual(uint16_t(0x0D), ParseAs<KeyboardTapAction>(R"({"action": "Keyboard.Tap", "key": "enter"})").Key);
      Assert::AreEqual(uint16_t(0x11), ParseAs<KeyboardTapAction>(R"({"action": "Keyboard.Tap", "key": "Ctrl"})").Key);
      Assert::AreEqual(uint16_t(0x31), ParseAs<KeyboardTapAction>(R"({"action": "Keyboard.Tap", "key": "1"})").Key, L"a digit is the key above the letters");
      Assert::AreEqual(uint16_t(0x70), ParseAs<KeyboardTapAction>(R"({"action": "Keyboard.Tap", "key": 112})").Key, L"a number is a virtual key code");
    }

    TEST_METHOD(InvalidActionsAreExplained)
    {
      AssertContains(ParseError(R"([1, 2])"), "must be an object");
      AssertContains(ParseError(R"({"x": 1})"), "\"action\" property");
      AssertContains(ParseError(R"({"action": "Mouse.Scroll"})"), "Unknown action 'Mouse.Scroll'");
      AssertContains(ParseError(R"({"action": "Mouse.Scroll"})"), "Mouse.Wheel");
      AssertContains(ParseError(R"({"action": "Mouse.Click", "buton": "Left"})"), "unknown property 'buton'");
      AssertContains(ParseError(R"({"action": "Mouse.Move", "x": 10})"), "'y' is required");
      AssertContains(ParseError(R"({"action": "Mouse.Move", "x": "10", "y": 10})"), "'x': Must be a number");
      AssertContains(ParseError(R"({"action": "Mouse.Move", "x": -5, "y": 10})"), "'x': Must be at least 0");
      AssertContains(ParseError(R"({"action": "Mouse.Click", "button": "Side"})"), "Must be one of: Left, Right, Middle");
      AssertContains(ParseError(R"({"action": "Keyboard.Tap", "key": "Hyper"})"), "Must be a key name");
      AssertContains(ParseError(R"({"action": "Window.Focus", "window": "not-a-window"})"), "window");
      AssertContains(ParseError(R"({"action": "Clipboard.Write", "content": []})"), "at least 1");
    }

    TEST_METHOD(ActionsRoundtripPolymorphically)
    {
      for (auto& [type, text] : _examples)
      {
        auto action = Parse(text);
        auto json = json_serializer<value_ptr<Action>>::to_json(action);

        value_ptr<Action> parsed;
        Assert::IsTrue(json_serializer<value_ptr<Action>>::from_json(json.get(), parsed), to_wstring(text).c_str());
        Assert::IsTrue(parsed->Type() == type, to_wstring(text).c_str());
        Assert::AreEqual(JsonSnapshot::ToCanonicalString(*json), JsonSnapshot::ToCanonicalString(*json_serializer<value_ptr<Action>>::to_json(parsed)), to_wstring(text).c_str());
      }
    }

    TEST_METHOD(SchemaMatchesSnapshot)
    {
      JsonSnapshot::AssertMatches("ActionSchema", *ActionCatalog::Schema());
    }

    TEST_METHOD(ResultSchemaMatchesSnapshot)
    {
      JsonSnapshot::AssertMatches("ActionResultSchema", *ActionCatalog::ResultSchema());
    }
  };
}
