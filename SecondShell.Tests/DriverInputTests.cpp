#include "pch.h"
#include "ActionCatalog.h"
#include "KeyboardActionHandler.h"
#include "MotionAnimator.h"
#include "MouseActionHandler.h"
#include "PenActionHandler.h"
#include "TouchActionHandler.h"

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

    //"touch 1 down 10,20 | 2 moved 30,40"
    class FakeTouchInjector : public ITouchInjector
    {
    public:
      explicit FakeTouchInjector(InputRecord& record) : _record(record) { }

      virtual void Inject(span<const TouchContact> frame) override
      {
        string event = "touch";
        for (auto& contact : frame)
        {
          auto phase = contact.Phase == TouchPhase::Down ? "down" : contact.Phase == TouchPhase::Up ? "up" : "moved";
          event += format("{} {} {} {},{}", event == "touch" ? "" : " |", contact.Id, phase, contact.Position.X, contact.Position.Y);
        }
        _record.Events.push_back(event);
      }

    private:
      InputRecord& _record;
    };

    //"pen out", "pen hover 10,20", "pen contact 10,20 pressure 0.50 barrel eraser"
    class FakePenInjector : public IPenInjector
    {
    public:
      explicit FakePenInjector(InputRecord& record) : _record(record) { }

      virtual void Inject(const PenState& state) override
      {
        if (!state.IsInRange) return _record.Events.push_back("pen out");

        auto event = format("pen {} {},{}", state.IsInContact ? "contact" : "hover", state.Position.X, state.Position.Y);
        if (state.IsInContact) event += format(" pressure {:.2f}", state.Pressure);
        if (state.IsBarrelPressed) event += " barrel";
        if (state.IsEraser) event += " eraser";
        _record.Events.push_back(event);
      }

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

    value_ptr<ActionResult> Touch(string_view text)
    {
      return _container.resolve<TouchActionHandler>()->Execute(*Parse(text), _noCancellation);
    }

    value_ptr<ActionResult> Pen(string_view text)
    {
      return _container.resolve<PenActionHandler>()->Execute(*Parse(text), _noCancellation);
    }

  public:
    DriverInputTests()
    {
      _container.add<IMouseInjector>(make_shared<FakeMouseInjector>(_record));
      _container.add<IKeyboardInjector>(make_shared<FakeKeyboardInjector>(_record));
      _container.add<ITouchInjector>(make_shared<FakeTouchInjector>(_record));
      _container.add<IPenInjector>(make_shared<FakePenInjector>(_record));
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

    TEST_METHOD(ReleasingAWindowsOrAltKeyOpensNoMenu)
    {
      Keyboard(R"({"action": "Keyboard.Press", "key": "LWin"})");
      _record.Events.clear();

      _container.resolve<InputStateTracker>()->ReleaseAll();
      Assert::IsTrue(_record.Events == vector<string>{ "key down 232", "key up 232", "key up 91" }, L"the mask key comes before the release");

      Keyboard(R"({"action": "Keyboard.Press", "key": "A"})");
      _record.Events.clear();
      _container.resolve<InputStateTracker>()->ReleaseAll();
      Assert::IsTrue(_record.Events == vector<string>{ "key up 65" }, L"other keys need no mask");
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

    TEST_METHOD(TapPutsAFingerDownAndLiftsIt)
    {
      Assert::IsTrue(Touch(R"({"action": "Touch.Tap", "id": 1, "x": 10, "y": 20})")->Status == ActionStatus::Succeeded);
      Assert::IsTrue(_record.Events == vector<string>{ "touch 1 down 10,20", "touch 1 up 10,20" });
    }

    TEST_METHOD(EveryTouchFrameHasAllFingers)
    {
      Touch(R"({"action": "Touch.Press", "id": 1, "x": 10, "y": 20})");
      Touch(R"({"action": "Touch.Press", "id": 2, "x": 30, "y": 40})");
      Touch(R"({"action": "Touch.Move", "id": 1, "x": 15, "y": 25})");
      Touch(R"({"action": "Touch.Release", "id": 2})");
      Assert::IsTrue(_record.Events == vector<string>{
        "touch 1 down 10,20",
        "touch 1 moved 10,20 | 2 down 30,40",
        "touch 1 moved 15,25 | 2 moved 30,40",
        "touch 1 moved 15,25 | 2 up 30,40" });
    }

    TEST_METHOD(TouchPointsMustBeDownOnlyOnce)
    {
      Touch(R"({"action": "Touch.Press", "id": 1, "x": 10, "y": 20})");
      Assert::IsTrue(Touch(R"({"action": "Touch.Press", "id": 1, "x": 30, "y": 40})")->Error == ErrorCode::InvalidArgument, L"the id is in use");
      Assert::IsTrue(Touch(R"({"action": "Touch.Tap", "id": 1, "x": 30, "y": 40})")->Error == ErrorCode::InvalidArgument, L"a tap too");
      Assert::IsTrue(Touch(R"({"action": "Touch.Move", "id": 2, "x": 30, "y": 40})")->Error == ErrorCode::InvalidArgument, L"2 is not down");
      Assert::IsTrue(Touch(R"({"action": "Touch.Release", "id": 2})")->Error == ErrorCode::InvalidArgument, L"nor can it be lifted");
      Assert::IsTrue(Touch(R"({"action": "Touch.Press", "id": 3, "x": 1920, "y": 40})")->Error == ErrorCode::InvalidArgument, L"off the screen");
      Assert::AreEqual(size_t(1), _record.Events.size(), L"only the first press is injected");
    }

    TEST_METHOD(TouchMovesAreAnimated)
    {
      Touch(R"({"action": "Touch.Press", "id": 1, "x": 0, "y": 0})");
      Touch(R"({"action": "Touch.Move", "id": 1, "x": 100, "y": 50, "duration": 50})");
      Assert::IsTrue(_record.Events.size() > 3, L"the move takes several steps");
      Assert::AreEqual(string("touch 1 moved 100,50"), _record.Events.back());
    }

    TEST_METHOD(ThePenHoversThenTouches)
    {
      Assert::IsTrue(Pen(R"({"action": "Pen.Press"})")->Error == ErrorCode::InvalidArgument, L"out of range, the pen needs a position");

      Pen(R"({"action": "Pen.Move", "x": 10, "y": 20})");
      Pen(R"({"action": "Pen.Press", "pressure": 0.3})");
      Pen(R"({"action": "Pen.Release"})");
      Pen(R"({"action": "Pen.Reset"})");
      Assert::IsTrue(_record.Events == vector<string>{
        "pen hover 10,20",
        "pen contact 10,20 pressure 0.30",
        "pen hover 10,20",
        "pen out" });
    }

    TEST_METHOD(PenStrokesChangePressureGradually)
    {
      Pen(R"({"action": "Pen.Press", "x": 0, "y": 0, "pressure": 0.2})");
      Pen(R"({"action": "Pen.Move", "x": 100, "y": 0, "pressure": 0.8, "duration": 50})");
      Assert::AreEqual(string("pen contact 100,0 pressure 0.80"), _record.Events.back());
      Assert::IsTrue(ranges::any_of(_record.Events, [](const string& event) {
        return event.find("contact") != string::npos && event.find("pressure 0.20") == string::npos && event.find("pressure 0.80") == string::npos;
        }), L"with pressures in between");
    }

    TEST_METHOD(TheEraserAndBarrelAreHeldFromHoverToContact)
    {
      Pen(R"({"action": "Pen.Tap", "x": 5, "y": 6, "eraser": true, "barrel": true})");
      Assert::IsTrue(_record.Events == vector<string>{
        "pen hover 5,6 barrel eraser",
        "pen contact 5,6 pressure 0.50 barrel eraser",
        "pen hover 5,6 eraser" });

      Assert::IsTrue(Pen(R"({"action": "Pen.Release"})")->Error == ErrorCode::InvalidArgument, L"it no longer touches the screen");
    }

    TEST_METHOD(FingersAndThePenAreReleasedAndReported)
    {
      Touch(R"({"action": "Touch.Press", "id": 2, "x": 10, "y": 20})");
      Touch(R"({"action": "Touch.Press", "id": 1, "x": 30, "y": 40})");
      Pen(R"({"action": "Pen.Press", "x": 50, "y": 60})");
      _record.Events.clear();

      auto released = _container.resolve<InputStateTracker>()->ReleaseAll();
      Assert::IsTrue(released.TouchPoints == vector<uint32_t>{ 2, 1 });
      Assert::IsTrue(released.Pen == optional<bool>{ true });
      Assert::IsTrue(_record.Events == vector<string>{ "touch 2 up 10,20 | 1 up 30,40", "pen hover 50,60", "pen out" }, L"lifted, then out of range");

      released = _container.resolve<InputStateTracker>()->ReleaseAll();
      Assert::IsTrue(released.TouchPoints.empty() && !released.Pen, L"nothing is held any more");
    }
  };
}
