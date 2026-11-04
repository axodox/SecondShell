#include "pch.h"
#include "ActionResult.h"
#include "CaptureActions.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "KeyboardActions.h"
#include "KeyNames.h"
#include "MouseActions.h"
#include "PenActions.h"
#include "ProcessActions.h"
#include "TouchActions.h"
#include "WindowActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace std;
using namespace std::chrono;

//All action and result descriptors are defined in this file: a derived descriptor copies its base's properties and
//registers itself with the base while it is constructed, so the bases must be constructed first, which C++ only
//guarantees within one translation unit.
namespace SecondShell::Actions
{
  namespace
  {
    const char* const _x = "Session pixels from the left edge of the session's desktop.";
    const char* const _y = "Session pixels from the top edge of the session's desktop.";
    const char* const _width = "Session pixels.";
    const char* const _height = "Session pixels.";
    const char* const _duration = "Milliseconds the input takes; with async it runs in parallel with the next actions.";
    const char* const _async = "Run in parallel with the next actions, until Control.Join.";
    const char* const _window = "A window ID from Window.Enumerate, e.g. \"0x0001A2B4\". IDs change when the application restarts; title, processName and pid find the window instead.";
    const char* const _windowTitle = "Instead of window: the topmost window whose title contains this text, ignoring case; dialogs included.";
    const char* const _windowProcessName = "Instead of window: the topmost window of this process, e.g. \"notepad.exe\" or \"notepad\". With title or pid, all must match.";
    const char* const _windowProcessId = "Instead of window: the topmost window of this process ID, e.g. from Process.Run.";
    const char* const _touchId = "Identifies the touch point; it must not be in use by another active touch point.";
    const char* const _pressure = "How hard the pen presses, 0..1; default 0.5 when it touches the screen.";
    const char* const _tiltX = "Degrees the pen leans right (positive) or left, -90..90; default 0.";
    const char* const _tiltY = "Degrees the pen leans toward the user (positive) or away, -90..90; default 0.";
    const char* const _rotation = "Degrees the pen is turned clockwise around its axis, 0..359; default 0.";
    const char* const _barrel = "Holds the pen's barrel button, which applications usually treat as a right click.";
    const char* const _eraser = "Uses the pen's eraser end instead of its tip.";
    const char* const _key ="One key: a key name of System.Windows.Forms.Keys (e.g. Enter, Tab, F5, A, D1, Left), an alias (Ctrl, Alt, Shift, Win, Esc, Backspace, 0-9), or a virtual key code number. Not a combination: for Ctrl+A, key A with modifiers [\"Ctrl\"].";
    const char* const _modifiers = "Modifier keys held during the key: Shift, Ctrl, Alt, Win.";
    const char* const _timeout = "Milliseconds to wait at most, default 10000; when it expires the sequence stops with a Timeout error.";
    const char* const _processId = "The process ID, e.g. from Process.Run or Process.Enumerate.";

    optional<milliseconds> ToMilliseconds(const optional<uint32_t>& value)
    {
      if (!value) return nullopt;
      return milliseconds(*value);
    }
  }

#pragma region Action
  json_object_descriptor<Action> Action::json_description = describe_json_object<Action>({ .name = "Action", .type_discriminator = "action" }, {});

  ActionKind Action::Kind() const
  {
    return ActionKind::None;
  }

  ActionType Action::Type() const
  {
    return ActionType::None;
  }

  bool Action::IsAsync() const
  {
    return false;
  }

  optional<milliseconds> Action::WaitTimeout() const
  {
    return nullopt;
  }

  const char* Action::Name() const
  {
    return json_serializer<Action>::get_type_description(*this)->name();
  }

  expected<> Action::Validate() const
  {
    return {};
  }

  json_object_descriptor<CaptureAction> CaptureAction::json_description = describe_json_object<CaptureAction, Action>(
    { .name = "Capture" },
    {
      { &CaptureAction::Tag, "tag", { .is_required = false, .schema = { .description = "A label returned with the capture; default: its index in the sequence." } } },
      { &CaptureAction::Output, "output", { .is_required = false, .schema = { .description = "Inline (default): returned as an image in the result. File: saved, the result has the path. Both." } } },
      { &CaptureAction::Path, "path", { .is_required = false, .schema = { .description = "File path for File and Both; default: a file in the session's capture folder." } } },
      { &CaptureAction::Format, "format", { .is_required = false, .schema = { .description = "Png (default) or Jpeg; inline captures are always PNG." } } },
      { &CaptureAction::Resolution, "resolution", { .is_required = false, .schema = { .description = "Largest image size: the capture is scaled down to fit, never up; the result reports the scale." } } }
    });
#pragma endregion

#pragma region Control
  json_object_descriptor<ControlWaitAction> ControlWaitAction::json_description = describe_json_object<ControlWaitAction, Action>(
    { .name = "Control.Wait", .description = "Waits a fixed time before the next action, e.g. {\"action\": \"Control.Wait\", \"duration\": 500}. Capture.WaitForStable and Capture.WaitForText wait only as long as needed." },
    {
      { &ControlWaitAction::Duration, "duration", { .is_required = true, .schema = { .description = "Milliseconds.", .minimum = 0 } } }
    });

  json_object_descriptor<ControlJoinAction> ControlJoinAction::json_description = describe_json_object<ControlJoinAction, Action>(
    { .name = "Control.Join", .description = "Waits for all async actions to complete." }, {});

  json_object_descriptor<ControlResetAction> ControlResetAction::json_description = describe_json_object<ControlResetAction, Action>(
    { .name = "Control.Reset", .description = "Cancels pending async actions and releases all held mouse buttons, keys and touch points, and lifts the pen. Does not move the cursor." }, {});
#pragma endregion

#pragma region Mouse
  json_object_descriptor<MouseMoveAction> MouseMoveAction::json_description = describe_json_object<MouseMoveAction, Action>(
    { .name = "Mouse.Move", .description = "Moves the mouse cursor to (x, y)." },
    {
      { &MouseMoveAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &MouseMoveAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } },
      { &MouseMoveAction::Duration, "duration", { .is_required = false, .schema = { .description = _duration, .minimum = 0 } } },
      { &MouseMoveAction::Async, "async", { .is_required = false, .schema = { .description = _async } } }
    });

  bool MouseMoveAction::IsAsync() const
  {
    return Async.value_or(false);
  }

  json_object_descriptor<MouseClickAction> MouseClickAction::json_description = describe_json_object<MouseClickAction, Action>(
    { .name = "Mouse.Click", .description = "Clicks a mouse button; with x and y the cursor first moves there instantly. For Ctrl+click and the like, hold the key with Keyboard.Press around it and let go with Keyboard.Release; for a triple click, Mouse.DoubleClick then Mouse.Click." },
    {
      { &MouseClickAction::Button, "button", { .is_required = false, .schema = { .description = "Left (default), Right or Middle." } } },
      { &MouseClickAction::X, "x", { .is_required = false, .schema = { .description = _x, .minimum = 0 } } },
      { &MouseClickAction::Y, "y", { .is_required = false, .schema = { .description = _y, .minimum = 0 } } },
      { &MouseClickAction::Duration, "duration", { .is_required = false, .schema = { .description = "Milliseconds to hold the button, for a long press.", .minimum = 0 } } },
      { &MouseClickAction::Async, "async", { .is_required = false, .schema = { .description = _async } } }
    });

  bool MouseClickAction::IsAsync() const
  {
    return Async.value_or(false);
  }

  json_object_descriptor<MouseDoubleClickAction> MouseDoubleClickAction::json_description = describe_json_object<MouseDoubleClickAction, Action>(
    { .name = "Mouse.DoubleClick", .description = "Double-clicks a mouse button; with x and y the cursor first moves there instantly." },
    {
      { &MouseDoubleClickAction::Button, "button", { .is_required = false, .schema = { .description = "Left (default), Right or Middle." } } },
      { &MouseDoubleClickAction::X, "x", { .is_required = false, .schema = { .description = _x, .minimum = 0 } } },
      { &MouseDoubleClickAction::Y, "y", { .is_required = false, .schema = { .description = _y, .minimum = 0 } } }
    });

  json_object_descriptor<MouseWheelAction> MouseWheelAction::json_description = describe_json_object<MouseWheelAction, Action>(
    { .name = "Mouse.Wheel", .description = "Rotates the mouse wheel; it scrolls the element under the cursor, so x and y first move the cursor there." },
    {
      { &MouseWheelAction::Delta, "delta", { .is_required = true, .schema = { .description = "Notches, may be fractional. Vertical: positive scrolls up. Horizontal: positive scrolls right." } } },
      { &MouseWheelAction::Axis, "axis", { .is_required = false, .schema = { .description = "Vertical (default) or Horizontal." } } },
      { &MouseWheelAction::X, "x", { .is_required = false, .schema = { .description = _x, .minimum = 0 } } },
      { &MouseWheelAction::Y, "y", { .is_required = false, .schema = { .description = _y, .minimum = 0 } } }
    });

  json_object_descriptor<MousePressAction> MousePressAction::json_description = describe_json_object<MousePressAction, Action>(
    { .name = "Mouse.Press", .description = "Presses a mouse button where the cursor is, and holds it until Mouse.Release. To drag: Mouse.Move to the start, Mouse.Press, Mouse.Move to the end with a duration, Mouse.Release." },
    {
      { &MousePressAction::Button, "button", { .is_required = false, .schema = { .description = "Left (default), Right or Middle." } } }
    });

  json_object_descriptor<MouseReleaseAction> MouseReleaseAction::json_description = describe_json_object<MouseReleaseAction, Action>(
    { .name = "Mouse.Release", .description = "Releases a mouse button at the cursor." },
    {
      { &MouseReleaseAction::Button, "button", { .is_required = false, .schema = { .description = "Left (default), Right or Middle." } } }
    });

  json_object_descriptor<MouseResetAction> MouseResetAction::json_description = describe_json_object<MouseResetAction, Action>(
    { .name = "Mouse.Reset", .description = "Releases all mouse buttons; the cursor stays where it is." }, {});
#pragma endregion

#pragma region Touch
  json_object_descriptor<TouchPressAction> TouchPressAction::json_description = describe_json_object<TouchPressAction, Action>(
    { .name = "Touch.Press", .description = "Puts a finger down at (x, y) and keeps it there until Touch.Release." },
    {
      { &TouchPressAction::Id, "id", { .is_required = true, .schema = { .description = _touchId, .minimum = 0 } } },
      { &TouchPressAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &TouchPressAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } }
    });

  json_object_descriptor<TouchReleaseAction> TouchReleaseAction::json_description = describe_json_object<TouchReleaseAction, Action>(
    { .name = "Touch.Release", .description = "Lifts the finger of a touch point at its current position." },
    {
      { &TouchReleaseAction::Id, "id", { .is_required = true, .schema = { .description = "The touch point to lift.", .minimum = 0 } } }
    });

  json_object_descriptor<TouchTapAction> TouchTapAction::json_description = describe_json_object<TouchTapAction, Action>(
    { .name = "Touch.Tap", .description = "Taps at (x, y): a quick press and release." },
    {
      { &TouchTapAction::Id, "id", { .is_required = true, .schema = { .description = _touchId, .minimum = 0 } } },
      { &TouchTapAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &TouchTapAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } }
    });

  json_object_descriptor<TouchMoveAction> TouchMoveAction::json_description = describe_json_object<TouchMoveAction, Action>(
    { .name = "Touch.Move", .description = "Moves a pressed touch point to (x, y)." },
    {
      { &TouchMoveAction::Id, "id", { .is_required = true, .schema = { .description = "The touch point to move.", .minimum = 0 } } },
      { &TouchMoveAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &TouchMoveAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } },
      { &TouchMoveAction::Duration, "duration", { .is_required = false, .schema = { .description = _duration, .minimum = 0 } } },
      { &TouchMoveAction::Async, "async", { .is_required = false, .schema = { .description = _async } } }
    });

  bool TouchMoveAction::IsAsync() const
  {
    return Async.value_or(false);
  }

  json_object_descriptor<TouchResetAction> TouchResetAction::json_description = describe_json_object<TouchResetAction, Action>(
    { .name = "Touch.Reset", .description = "Lifts all touch points." }, {});
#pragma endregion

#pragma region Pen
  json_object_descriptor<PenMoveAction> PenMoveAction::json_description = describe_json_object<PenMoveAction, Action>(
    { .name = "Pen.Move", .description = "Moves the pen to (x, y): touching the screen it draws, otherwise it hovers. A pen out of range comes into range above (x, y). Pressure, tilt and rotation change gradually to the values given; the others stay." },
    {
      { &PenMoveAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &PenMoveAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } },
      { &PenMoveAction::Duration, "duration", { .is_required = false, .schema = { .description = _duration, .minimum = 0 } } },
      { &PenMoveAction::Async, "async", { .is_required = false, .schema = { .description = _async } } },
      { &PenMoveAction::Pressure, "pressure", { .is_required = false, .schema = { .description = _pressure, .minimum = 0, .maximum = 1 } } },
      { &PenMoveAction::TiltX, "tiltX", { .is_required = false, .schema = { .description = _tiltX, .minimum = -90, .maximum = 90 } } },
      { &PenMoveAction::TiltY, "tiltY", { .is_required = false, .schema = { .description = _tiltY, .minimum = -90, .maximum = 90 } } },
      { &PenMoveAction::Rotation, "rotation", { .is_required = false, .schema = { .description = _rotation, .minimum = 0, .maximum = 359 } } }
    });

  bool PenMoveAction::IsAsync() const
  {
    return Async.value_or(false);
  }

  json_object_descriptor<PenPressAction> PenPressAction::json_description = describe_json_object<PenPressAction, Action>(
    { .name = "Pen.Press", .description = "Touches the screen with the pen where it hovers, or at (x, y), and keeps it there until Pen.Release." },
    {
      { &PenPressAction::X, "x", { .is_required = false, .schema = { .description = _x, .minimum = 0 } } },
      { &PenPressAction::Y, "y", { .is_required = false, .schema = { .description = _y, .minimum = 0 } } },
      { &PenPressAction::Pressure, "pressure", { .is_required = false, .schema = { .description = _pressure, .minimum = 0, .maximum = 1 } } },
      { &PenPressAction::TiltX, "tiltX", { .is_required = false, .schema = { .description = _tiltX, .minimum = -90, .maximum = 90 } } },
      { &PenPressAction::TiltY, "tiltY", { .is_required = false, .schema = { .description = _tiltY, .minimum = -90, .maximum = 90 } } },
      { &PenPressAction::Rotation, "rotation", { .is_required = false, .schema = { .description = _rotation, .minimum = 0, .maximum = 359 } } },
      { &PenPressAction::Barrel, "barrel", { .is_required = false, .schema = { .description = _barrel } } },
      { &PenPressAction::Eraser, "eraser", { .is_required = false, .schema = { .description = _eraser } } }
    });

  json_object_descriptor<PenReleaseAction> PenReleaseAction::json_description = describe_json_object<PenReleaseAction, Action>(
    { .name = "Pen.Release", .description = "Lifts the pen off the screen; it keeps hovering there until Pen.Reset." }, {});

  json_object_descriptor<PenTapAction> PenTapAction::json_description = describe_json_object<PenTapAction, Action>(
    { .name = "Pen.Tap", .description = "Taps with the pen at (x, y); it keeps hovering there until Pen.Reset." },
    {
      { &PenTapAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &PenTapAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } },
      { &PenTapAction::Pressure, "pressure", { .is_required = false, .schema = { .description = _pressure, .minimum = 0, .maximum = 1 } } },
      { &PenTapAction::Barrel, "barrel", { .is_required = false, .schema = { .description = _barrel } } },
      { &PenTapAction::Eraser, "eraser", { .is_required = false, .schema = { .description = _eraser } } }
    });

  json_object_descriptor<PenResetAction> PenResetAction::json_description = describe_json_object<PenResetAction, Action>(
    { .name = "Pen.Reset", .description = "Lifts the pen and takes it out of range." }, {});
#pragma endregion

#pragma region Keyboard
  json_object_descriptor<KeyboardTypeAction> KeyboardTypeAction::json_description = describe_json_object<KeyboardTypeAction, Action>(
    { .name = "Keyboard.Type", .description = "Types text as Unicode characters, independent of the keyboard layout. \\n is typed as Enter and \\t as Tab; use Keyboard.Tap for other keys." },
    {
      { &KeyboardTypeAction::Text, "text", { .is_required = true } },
      { &KeyboardTypeAction::Pause, "pause", { .is_required = false, .schema = { .description = "Milliseconds between keystrokes.", .minimum = 0 } } },
      { &KeyboardTypeAction::Sensitive, "sensitive", { .is_required = false, .schema = { .description = "Hide the text from the action log and the on-screen visualization, e.g. for passwords." } } }
    });

  json_object_descriptor<KeyboardTapAction> KeyboardTapAction::json_description = describe_json_object<KeyboardTapAction, Action>(
    { .name = "Keyboard.Tap", .description = "Presses and releases a key, with modifiers held, e.g. key S with modifiers [\"Ctrl\"] for Ctrl+S." },
    {
      { &KeyboardTapAction::Key, "key", KeyConverter{}, { .is_required = true, .schema = { .description = _key } } },
      { &KeyboardTapAction::Modifiers, "modifiers", { .is_required = false, .schema = { .description = _modifiers } } }
    });

  json_object_descriptor<KeyboardPressAction> KeyboardPressAction::json_description = describe_json_object<KeyboardPressAction, Action>(
    { .name = "Keyboard.Press", .description = "Presses the modifiers, then the key, and holds them until Keyboard.Release." },
    {
      { &KeyboardPressAction::Key, "key", KeyConverter{}, { .is_required = true, .schema = { .description = _key } } },
      { &KeyboardPressAction::Modifiers, "modifiers", { .is_required = false, .schema = { .description = _modifiers } } }
    });

  json_object_descriptor<KeyboardReleaseAction> KeyboardReleaseAction::json_description = describe_json_object<KeyboardReleaseAction, Action>(
    { .name = "Keyboard.Release", .description = "Releases the key, then the modifiers." },
    {
      { &KeyboardReleaseAction::Key, "key", KeyConverter{}, { .is_required = true, .schema = { .description = _key } } },
      { &KeyboardReleaseAction::Modifiers, "modifiers", { .is_required = false, .schema = { .description = _modifiers } } }
    });

  json_object_descriptor<KeyboardResetAction> KeyboardResetAction::json_description = describe_json_object<KeyboardResetAction, Action>(
    { .name = "Keyboard.Reset", .description = "Releases all pressed keys." }, {});
#pragma endregion

#pragma region Capture
  json_object_descriptor<CaptureScreenAction> CaptureScreenAction::json_description = describe_json_object<CaptureScreenAction, CaptureAction>(
    { .name = "Capture.Screen", .description = "Captures the session's whole desktop. For a smaller image, set resolution, e.g. {\"width\": 1280, \"height\": 720}; the result reports the scale." }, {});

  json_object_descriptor<CaptureWindowAction> CaptureWindowAction::json_description = describe_json_object<CaptureWindowAction, CaptureAction>(
    { .name = "Capture.Window", .description = "Captures one window, by default the active one; prefer it over Capture.Screen to save context." },
    {
      { &CaptureWindowAction::Window, "window", { .is_required = false, .schema = { .description = _window } } },
      { &CaptureWindowAction::Title, "title", { .is_required = false, .schema = { .description = _windowTitle } } },
      { &CaptureWindowAction::ProcessName, "processName", { .is_required = false, .schema = { .description = _windowProcessName } } },
      { &CaptureWindowAction::ProcessId, "pid", { .is_required = false, .schema = { .description = _windowProcessId, .minimum = 0 } } }
    });

  WindowQuery CaptureWindowAction::Query() const
  {
    return { Window, Title, ProcessName, ProcessId };
  }

  expected<> CaptureWindowAction::Validate() const
  {
    return Query().Validate(false);
  }

  json_object_descriptor<CaptureRegionAction> CaptureRegionAction::json_description = describe_json_object<CaptureRegionAction, CaptureAction>(
    { .name = "Capture.Region", .description = "Captures a rectangle of the session's desktop." },
    {
      { &CaptureRegionAction::X, "x", { .is_required = true, .schema = { .description = _x, .minimum = 0 } } },
      { &CaptureRegionAction::Y, "y", { .is_required = true, .schema = { .description = _y, .minimum = 0 } } },
      { &CaptureRegionAction::Width, "width", { .is_required = true, .schema = { .description = _width, .minimum = 1 } } },
      { &CaptureRegionAction::Height, "height", { .is_required = true, .schema = { .description = _height, .minimum = 1 } } }
    });

  json_object_descriptor<CaptureWaitForTextAction> CaptureWaitForTextAction::json_description = describe_json_object<CaptureWaitForTextAction, Action>(
    { .name = "Capture.WaitForText", .description = "Waits until the text appears on the screen, found with UI Automation and OCR (forgiving small misreads, also across line breaks), so it may not be 100% accurate." },
    {
      { &CaptureWaitForTextAction::Text, "text", { .is_required = true } },
      { &CaptureWaitForTextAction::Interval, "interval", { .is_required = false, .schema = { .description = "Milliseconds between checks, default 250.", .minimum = 1 } } },
      { &CaptureWaitForTextAction::Timeout, "timeout", { .is_required = false, .schema = { .description = _timeout, .minimum = 0 } } }
    });

  optional<milliseconds> CaptureWaitForTextAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }

  json_object_descriptor<CaptureWaitForStableAction> CaptureWaitForStableAction::json_description = describe_json_object<CaptureWaitForStableAction, Action>(
    { .name = "Capture.WaitForStable", .description = "Waits until the screen stops changing; use it instead of a fixed Control.Wait after an action whose effect takes an unknown time. Animations such as a blinking caret keep the screen changing, so limit the check to a window or region." },
    {
      { &CaptureWaitForStableAction::Stable, "stable", { .is_required = false, .schema = { .description = "Milliseconds the screen must stay the same, default 500.", .minimum = 1 } } },
      { &CaptureWaitForStableAction::Window, "window", { .is_required = false, .schema = { .description = "Only check this window." } } },
      { &CaptureWaitForStableAction::X, "x", { .is_required = false, .schema = { .description = "Only check this region: left edge in session pixels.", .minimum = 0 } } },
      { &CaptureWaitForStableAction::Y, "y", { .is_required = false, .schema = { .description = "Only check this region: top edge in session pixels.", .minimum = 0 } } },
      { &CaptureWaitForStableAction::Width, "width", { .is_required = false, .schema = { .description = "Only check this region: width in session pixels.", .minimum = 1 } } },
      { &CaptureWaitForStableAction::Height, "height", { .is_required = false, .schema = { .description = "Only check this region: height in session pixels.", .minimum = 1 } } },
      { &CaptureWaitForStableAction::Interval, "interval", { .is_required = false, .schema = { .description = "Milliseconds between checks, default 100.", .minimum = 1 } } },
      { &CaptureWaitForStableAction::Timeout, "timeout", { .is_required = false, .schema = { .description = _timeout, .minimum = 0 } } }
    });

  optional<milliseconds> CaptureWaitForStableAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }
#pragma endregion

#pragma region Window
  json_object_descriptor<WindowEnumerateAction> WindowEnumerateAction::json_description = describe_json_object<WindowEnumerateAction, Action>(
    { .name = "Window.Enumerate", .description = "Lists the open top-level windows, topmost first: ID, title, process, position, size, state, and which one is in the foreground." },
    {
      { &WindowEnumerateAction::All, "all", { .is_required = false, .schema = { .description = "Also list dialogs and other windows the taskbar does not show, if they have a title." } } }
    });

  json_object_descriptor<WindowTargetAction> WindowTargetAction::json_description = describe_json_object<WindowTargetAction, Action>(
    { .name = "WindowTarget" },
    {
      { &WindowTargetAction::Window, "window", { .is_required = false, .schema = { .description = _window } } },
      { &WindowTargetAction::Title, "title", { .is_required = false, .schema = { .description = _windowTitle } } },
      { &WindowTargetAction::ProcessName, "processName", { .is_required = false, .schema = { .description = _windowProcessName } } },
      { &WindowTargetAction::ProcessId, "pid", { .is_required = false, .schema = { .description = _windowProcessId, .minimum = 0 } } }
    });

  WindowQuery WindowTargetAction::Query() const
  {
    return { Window, Title, ProcessName, ProcessId };
  }

  expected<> WindowTargetAction::Validate() const
  {
    return Query().Validate(true);
  }

  json_object_descriptor<WindowFocusAction> WindowFocusAction::json_description = describe_json_object<WindowFocusAction, WindowTargetAction>(
    { .name = "Window.Focus", .description = "Brings a window to the foreground and gives it keyboard focus, restoring it if minimized. Use it before typing into a window, e.g. {\"action\": \"Window.Focus\", \"title\": \"Notepad\"}." }, {});

  json_object_descriptor<WindowMoveAction> WindowMoveAction::json_description = describe_json_object<WindowMoveAction, WindowTargetAction>(
    { .name = "Window.Move", .description = "Moves a window's top-left corner to (x, y)." },
    {
      { &WindowMoveAction::X, "x", { .is_required = true, .schema = { .description = _x } } },
      { &WindowMoveAction::Y, "y", { .is_required = true, .schema = { .description = _y } } }
    });

  json_object_descriptor<WindowResizeAction> WindowResizeAction::json_description = describe_json_object<WindowResizeAction, WindowTargetAction>(
    { .name = "Window.Resize", .description = "Resizes a window." },
    {
      { &WindowResizeAction::Width, "width", { .is_required = true, .schema = { .description = _width, .minimum = 1 } } },
      { &WindowResizeAction::Height, "height", { .is_required = true, .schema = { .description = _height, .minimum = 1 } } }
    });

  json_object_descriptor<WindowMinimizeAction> WindowMinimizeAction::json_description = describe_json_object<WindowMinimizeAction, WindowTargetAction>(
    { .name = "Window.Minimize", .description = "Minimizes a window." }, {});

  json_object_descriptor<WindowMaximizeAction> WindowMaximizeAction::json_description = describe_json_object<WindowMaximizeAction, WindowTargetAction>(
    { .name = "Window.Maximize", .description = "Maximizes a window." }, {});

  json_object_descriptor<WindowRestoreAction> WindowRestoreAction::json_description = describe_json_object<WindowRestoreAction, WindowTargetAction>(
    { .name = "Window.Restore", .description = "Restores a window to its previous size and position." }, {});

  json_object_descriptor<WindowCloseAction> WindowCloseAction::json_description = describe_json_object<WindowCloseAction, WindowTargetAction>(
    { .name = "Window.Close", .description = "Asks a window to close, as its X button does; the application may ask for confirmation first." }, {});
#pragma endregion

#pragma region Process
  json_object_descriptor<ProcessRunAction> ProcessRunAction::json_description = describe_json_object<ProcessRunAction, Action>(
    { .name = "Process.Run", .description = "Runs a command in the session. Synchronous by default: waits for the exit and returns the exit code and the output and error streams (each truncated to 64 KB). Use async for programs that keep running, e.g. GUI applications or a console to interact with: it starts the program as Explorer would and returns its process ID at once; with shell None the ID is the program's own. Start a packaged (Store) app through its execution alias with workingDirectory set to its package folder, as some load files relative to it." },
    {
      { &ProcessRunAction::Command, "command", { .is_required = true, .schema = { .description = "Without args: a command line for the shell, e.g. \"Get-ChildItem C:\\\\Temp | Select-Object -First 5\". With args, or with shell None: the program to run, e.g. \"cmd.exe\" or \"C:\\\\Tools\\\\tool.exe\" (in PowerShell also a script or cmdlet)." } } },
      { &ProcessRunAction::Shell, "shell", { .is_required = false, .schema = { .description = "PowerShell (Windows PowerShell), Pwsh (PowerShell 7), Cmd, or None (no shell: command is an executable); default: the user's setting, PowerShell unless they changed it." } } },
      { &ProcessRunAction::Args, "args", { .is_required = false, .schema = { .description = "Arguments of the program in command, each passed as it is, never parsed by the shell, e.g. [\"/c\", \"dir\"] with command \"cmd.exe\"." } } },
      { &ProcessRunAction::WorkingDirectory, "workingDirectory", { .is_required = false, .schema = { .description = "Default: the user's profile folder." } } },
      { &ProcessRunAction::Async, "async", { .is_required = false, .schema = { .description = "Start the program as Explorer would (console programs get a window, nothing is redirected) and return its process ID at once; Control.Join does not wait for it, Process.WaitForProcess does." } } },
      { &ProcessRunAction::Output, "output", { .is_required = false, .schema = { .description = "With async: collect the program's output, e.g. a server's log, instead of giving it a window, and take input. Process.ReadOutput, Process.WaitForOutput and Process.WriteInput then work with it; the result has the file the output goes to, and the user sees it live in SecondShell." } } },
      { &ProcessRunAction::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds to wait for the exit, default 10000; when it expires the sequence stops with a Timeout error and the process keeps running.", .minimum = 0 } } }
    });

  optional<milliseconds> ProcessRunAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }

  expected<> ProcessRunAction::Validate() const
  {
    if (Output.value_or(false) && !Async.value_or(false)) return failure<>("output needs async: a synchronous run returns its output anyway.");
    return {};
  }

  json_object_descriptor<ProcessReadOutputAction> ProcessReadOutputAction::json_description = describe_json_object<ProcessReadOutputAction, Action>(
    { .name = "Process.ReadOutput", .description = "Reads the output of a process started with output (both streams, as they arrived): by default its last 50 lines. With from, the output from that offset on, e.g. next of the previous read, to follow it. Also tells whether it still runs, and its exit code." },
    {
      { &ProcessReadOutputAction::ProcessId, "pid", { .is_required = true, .schema = { .description = _processId } } },
      { &ProcessReadOutputAction::Tail, "tail", { .is_required = false, .schema = { .description = "The last this many lines.", .minimum = 1 } } },
      { &ProcessReadOutputAction::From, "from", { .is_required = false, .schema = { .description = "An offset, as next of an earlier result returned; 0 is the start. Older output than the recent part kept in memory is read from its files.", .minimum = 0 } } },
      { &ProcessReadOutputAction::MaxBytes, "maxBytes", { .is_required = false, .schema = { .description = "Most bytes to return, default 16384; more follows from next.", .minimum = 1, .maximum = 1048576 } } }
    });

  expected<> ProcessReadOutputAction::Validate() const
  {
    if (Tail && From) return failure<>("give tail or from, not both.");
    return {};
  }

  json_object_descriptor<ProcessWaitForOutputAction> ProcessWaitForOutputAction::json_description = describe_json_object<ProcessWaitForOutputAction, Action>(
    { .name = "Process.WaitForOutput", .description = "Waits until a line of the output of a process started with output contains the text, e.g. a server's \"Listening on\"; returns that line, and next to read on from. Fails at once if the process exits first." },
    {
      { &ProcessWaitForOutputAction::ProcessId, "pid", { .is_required = true, .schema = { .description = _processId } } },
      { &ProcessWaitForOutputAction::Text, "text", { .is_required = true, .schema = { .description = "Found ignoring case." } } },
      { &ProcessWaitForOutputAction::From, "from", { .is_required = false, .schema = { .description = "Only look at output from this offset on; default 0: also what was written before the wait, as far as it is kept in memory.", .minimum = 0 } } },
      { &ProcessWaitForOutputAction::Timeout, "timeout", { .is_required = false, .schema = { .description = _timeout, .minimum = 0 } } }
    });

  optional<milliseconds> ProcessWaitForOutputAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }

  json_object_descriptor<ProcessWriteInputAction> ProcessWriteInputAction::json_description = describe_json_object<ProcessWriteInputAction, Action>(
    { .name = "Process.WriteInput", .description = "Writes text to the input of a process started with output, as UTF-8; end lines with \\n, as the program reads them. With close, the input ends after it, which tells many programs that nothing more comes." },
    {
      { &ProcessWriteInputAction::ProcessId, "pid", { .is_required = true, .schema = { .description = _processId } } },
      { &ProcessWriteInputAction::Text, "text", { .is_required = false } },
      { &ProcessWriteInputAction::Close, "close", { .is_required = false, .schema = { .description = "End the input after the text." } } }
    });

  json_object_descriptor<ProcessStopAction> ProcessStopAction::json_description = describe_json_object<ProcessStopAction, Action>(
    { .name = "Process.Stop", .description = "Asks a process to exit gracefully: its windows get a close request, console processes get Ctrl+C. Returns its exit code; use Process.Kill if it does not exit." },
    {
      { &ProcessStopAction::ProcessId, "pid", { .is_required = true, .schema = { .description = _processId } } },
      { &ProcessStopAction::Timeout, "timeout", { .is_required = false, .schema = { .description = _timeout, .minimum = 0 } } }
    });

  optional<milliseconds> ProcessStopAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }

  json_object_descriptor<ProcessKillAction> ProcessKillAction::json_description = describe_json_object<ProcessKillAction, Action>(
    { .name = "Process.Kill", .description = "Terminates a process immediately, without a chance to save or clean up." },
    {
      { &ProcessKillAction::ProcessId, "pid", { .is_required = true, .schema = { .description = _processId } } },
      { &ProcessKillAction::Tree, "tree", { .is_required = false, .schema = { .description = "Also terminate its child processes." } } }
    });

  json_object_descriptor<ProcessWaitForProcessAction> ProcessWaitForProcessAction::json_description = describe_json_object<ProcessWaitForProcessAction, Action>(
    { .name = "Process.WaitForProcess", .description = "Waits for a process to exit and returns its exit code." },
    {
      { &ProcessWaitForProcessAction::ProcessId, "pid", { .is_required = true, .schema = { .description = _processId } } },
      { &ProcessWaitForProcessAction::Timeout, "timeout", { .is_required = false, .schema = { .description = _timeout, .minimum = 0 } } }
    });

  optional<milliseconds> ProcessWaitForProcessAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }

  json_object_descriptor<ProcessEnumerateAction> ProcessEnumerateAction::json_description = describe_json_object<ProcessEnumerateAction, Action>(
    { .name = "Process.Enumerate", .description = "Lists the running processes of the session with their IDs, names, CPU and memory use." }, {});
#pragma endregion

#pragma region Clipboard
  json_object_descriptor<ClipboardReadAction> ClipboardReadAction::json_description = describe_json_object<ClipboardReadAction, Action>(
    { .name = "Clipboard.Read", .description = "Reads the session's clipboard (separate from the user's). Always lists the formats on the clipboard; without format also returns the text, if any. Images are returned like captures. Raw data is limited to 1 MB." },
    {
      { &ClipboardReadAction::Format, "format", { .is_required = false, .schema = { .description = "Text, Html, Image, Files or Raw." } } },
      { &ClipboardReadAction::Name, "name", { .is_required = false, .schema = { .description = "For Raw: a registered clipboard format name, a standard one such as CF_UNICODETEXT, or a format number." } } },
      { &ClipboardReadAction::Output, "output", { .is_required = false, .schema = { .description = "For images: Inline (default), File or Both." } } },
      { &ClipboardReadAction::Path, "path", { .is_required = false, .schema = { .description = "For images: the file path for File and Both." } } },
      { &ClipboardReadAction::Resolution, "resolution", { .is_required = false, .schema = { .description = "For images: the largest image size." } } }
    });

  json_object_descriptor<ClipboardWriteAction> ClipboardWriteAction::json_description = describe_json_object<ClipboardWriteAction, Action>(
    { .name = "Clipboard.Write", .description = "Replaces the session's clipboard content; offer several formats at once (e.g. Text and Html), as applications do." },
    {
      { &ClipboardWriteAction::Content, "content", { .is_required = true, .schema = { .min_items = 1 } } }
    });
#pragma endregion

#pragma region Results
  json_object_descriptor<ActionResult> ActionResult::json_description = describe_json_object<ActionResult>(
    { .name = "Status", .type_discriminator = "type" },
    {
      { &ActionResult::Status, "status" },
      { &ActionResult::Error, "error", { .is_required = false } },
      { &ActionResult::Message, "message", { .is_required = false } }
    });

  value_ptr<ActionResult> ActionResult::Succeeded()
  {
    return make_value<ActionResult>();
  }

  value_ptr<ActionResult> ActionResult::Cancelled()
  {
    auto result = make_value<ActionResult>();
    result->Status = ActionStatus::Cancelled;
    return result;
  }

  value_ptr<ActionResult> ActionResult::Failed(ErrorCode error, string message)
  {
    auto result = make_value<ActionResult>();
    result->Status = ActionStatus::Failed;
    result->Error = error;
    result->Message = move(message);
    return result;
  }

  json_object_descriptor<CaptureResult> CaptureResult::json_description = describe_json_object<CaptureResult, ActionResult>(
    { .name = "Capture" },
    {
      { &CaptureResult::Tag, "tag" },
      { &CaptureResult::Image, "image" }
    });

  json_object_descriptor<ProcessResult> ProcessResult::json_description = describe_json_object<ProcessResult, ActionResult>(
    { .name = "Process" },
    {
      { &ProcessResult::ProcessId, "pid" },
      { &ProcessResult::ExitCode, "exitCode", { .is_required = false } },
      { &ProcessResult::StandardOutput, "stdout", { .is_required = false } },
      { &ProcessResult::StandardError, "stderr", { .is_required = false } },
      { &ProcessResult::IsOutputTruncated, "stdoutTruncated", { .is_required = false } },
      { &ProcessResult::IsErrorTruncated, "stderrTruncated", { .is_required = false } },
      { &ProcessResult::OutputFile, "outputFile", { .is_required = false, .schema = { .description = "Where the output goes; past its size limit it continues in the same name with .2, .3 before .log." } } }
    });

  json_object_descriptor<ProcessOutputResult> ProcessOutputResult::json_description = describe_json_object<ProcessOutputResult, ActionResult>(
    { .name = "ProcessOutput" },
    {
      { &ProcessOutputResult::ProcessId, "pid" },
      { &ProcessOutputResult::Text, "text" },
      { &ProcessOutputResult::From, "from", { .schema = { .description = "The offset of the text." } } },
      { &ProcessOutputResult::Next, "next", { .schema = { .description = "The offset after the text: from of the next read." } } },
      { &ProcessOutputResult::IsRunning, "isRunning" },
      { &ProcessOutputResult::ExitCode, "exitCode", { .is_required = false } },
      { &ProcessOutputResult::IsTruncated, "truncated", { .is_required = false, .schema = { .description = "More output follows before next; read on from next." } } }
    });

  json_object_descriptor<ProcessListResult> ProcessListResult::json_description = describe_json_object<ProcessListResult, ActionResult>(
    { .name = "ProcessList" },
    {
      { &ProcessListResult::Processes, "processes" }
    });

  json_object_descriptor<WindowListResult> WindowListResult::json_description = describe_json_object<WindowListResult, ActionResult>(
    { .name = "WindowList" },
    {
      { &WindowListResult::Windows, "windows" }
    });

  json_object_descriptor<ClipboardResult> ClipboardResult::json_description = describe_json_object<ClipboardResult, ActionResult>(
    { .name = "Clipboard" },
    {
      { &ClipboardResult::Formats, "formats", { .is_required = false } },
      { &ClipboardResult::Content, "content", { .is_required = false } },
      { &ClipboardResult::Image, "image", { .is_required = false } }
    });
#pragma endregion
}
