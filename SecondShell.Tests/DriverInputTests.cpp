#include "pch.h"
#include "ActionCatalog.h"
#include "KeyboardActionHandler.h"
#include "MotionAnimator.h"
#include "MouseActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Driver;
using namespace SecondShell::Input;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  namespace
  {
    //Records the injected input as text, e.g. "move 10,20", "press Left", "key down 17", "char 65"
    struct InputRecord
    {
      vector<string> Events;
    };

    class FakeMouseInjector : public IMouseInjector
    {
    public:
      explicit FakeMouseInjector(InputRecord& record) : _record(record) { }

      virtual Point Position() const override { return _position; }
      virtual Point ScreenSize() const override { return { 1920, 1080 }; }

      virtual void MoveTo(Point position) override
      {
        _position = position;
        _record.Events.push_back(format("move {},{}", position.X, position.Y));
      }

      virtual void Press(MouseButton button) override { _record.Events.push_back("press " + to_string(button)); }
      virtual void Release(MouseButton button) override { _record.Events.push_back("release " + to_string(button)); }
      virtual void Wheel(float notches, WheelAxis axis) override { _record.Events.push_back(format("wheel {} {}", notches, to_string(axis))); }

    private:
      InputRecord& _record;
      Point _position;
    };

    class FakeKeyboardInjector : public IKeyboardInjector
    {
    public:
      explicit FakeKeyboardInjector(InputRecord& record) : _record(record) { }

      virtual void Press(uint16_t virtualKey) override { _record.Events.push_back(format("key down {}", virtualKey)); }
      virtual void Release(uint16_t virtualKey) override { _record.Events.push_back(format("key up {}", virtualKey)); }
      virtual void TypeCharacter(char32_t character) override { _record.Events.push_back(format("char {}", uint32_t(character))); }

    private:
      InputRecord& _record;
    };
  }

  TEST_CLASS(DriverInputTests)
  {
    InputRecord _record;
    dependency_container _container;
    CancellationToken _noCancellation;

    static value_ptr<Action> Parse(string_view text)
    {
      auto json = Axodox::Json::json_value::from_string(text);
      auto action = ActionCatalog::Parse(*json);
      if (!action) Assert::Fail(to_wstring(action.error()).c_str());
      return *action;
    }

    value_ptr<ActionResult> Mouse(string_view text)
    {
      return _container.resolve<MouseActionHandler>()->Execute(*Parse(text), _noCancellation);
    }

    value_ptr<ActionResult> Keyboard(string_view text)
    {
      return _container.resolve<KeyboardActionHandler>()->Execute(*Parse(text), _noCancellation);
    }

  public:
    DriverInputTests()
    {
      _container.add<IMouseInjector>(make_shared<FakeMouseInjector>(_record));
      _container.add<IKeyboardInjector>(make_shared<FakeKeyboardInjector>(_record));
    }

    TEST_METHOD(ClickMovesThenPressesAndReleases)
    {
      Assert::IsTrue(Mouse(R"({"action": "Mouse.Click", "button": "Right", "x": 100, "y": 200})")->Status == ActionStatus::Succeeded);
      Assert::IsTrue(_record.Events == vector<string>{ "move 100,200", "press Right", "release Right" });
    }

    TEST_METHOD(PositionsOutsideTheScreenAreRejected)
    {
      auto result = Mouse(R"({"action": "Mouse.Click", "x": 1920, "y": 5})");
      Assert::IsTrue(result->Error == ErrorCode::InvalidArgument);
      Assert::IsTrue(_record.Events.empty(), L"nothing is injected");

      result = Mouse(R"({"action": "Mouse.Wheel", "delta": 1, "x": 5})");
      Assert::IsTrue(result->Error == ErrorCode::InvalidArgument, L"x without y");
    }

    TEST_METHOD(MoveIsAnimatedToTheTarget)
    {
      Mouse(R"({"action": "Mouse.Move", "x": 100, "y": 50, "duration": 50})");
      Assert::IsTrue(_record.Events.size() > 2, L"the move takes several steps");
      Assert::AreEqual(string("move 100,50"), _record.Events.back());
    }

    TEST_METHOD(HeldInputIsReleasedAndReported)
    {
      Mouse(R"({"action": "Mouse.Press", "button": "Left"})");
      Keyboard(R"({"action": "Keyboard.Press", "key": "A", "modifiers": ["Ctrl"]})");

      auto released = _container.resolve<InputStateTracker>()->ReleaseAll();
      Assert::IsTrue(released.MouseButtons == vector<MouseButton>{ MouseButton::Left });
      Assert::IsTrue(released.Keys == vector<string>{ "ControlKey", "A" });
      Assert::AreEqual(string("key up 17"), _record.Events.back(), L"keys are released in reverse order");

      Assert::IsTrue(_container.resolve<InputStateTracker>()->ReleaseAll().Keys.empty(), L"nothing is held any more");
    }

    TEST_METHOD(TapPressesModifiersAroundTheKey)
    {
      Keyboard(R"({"action": "Keyboard.Tap", "key": "S", "modifiers": ["Ctrl", "Shift"]})");
      Assert::IsTrue(_record.Events == vector<string>{ "key down 17", "key down 16", "key down 83", "key up 83", "key up 16", "key up 17" });
    }

    TEST_METHOD(TypingSendsCharactersAndLineBreaksAsKeys)
    {
      Keyboard(R"({"action": "Keyboard.Type", "text": "Á😀\n", "pause": 0})");
      Assert::IsTrue(_record.Events == vector<string>{ "char 193", "char 128512", "key down 13", "key up 13" });
    }

    TEST_METHOD(AnimationEndsAtTheTargetOrStopsWhenCancelled)
    {
      vector<float> steps;
      Assert::IsTrue(MotionAnimator::Animate(0ms, [&](float progress) { steps.push_back(progress); }, _noCancellation));
      Assert::IsTrue(steps == vector<float>{ 1.f }, L"without a duration it jumps to the end");

      CancellationToken cancellation;
      cancellation.Cancel();
      steps.clear();
      Assert::IsFalse(MotionAnimator::Animate(1000ms, [&](float progress) { steps.push_back(progress); }, cancellation));
      Assert::IsTrue(steps.size() == 1 && steps[0] < 1.f, L"it stops after the first step");
    }
  };
}
