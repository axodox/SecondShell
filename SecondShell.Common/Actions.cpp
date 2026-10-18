#include "pch.h"
#include "ActionResult.h"
#include "CaptureActions.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "KeyboardActions.h"
#include "KeyNames.h"
#include "MouseActions.h"
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
    const char* const _window = "A window ID from Window.Enumerate, e.g. \"0x0001A2B4\".";
    const char* const _touchId = "Identifies the touch point; it must not be in use by another active touch point.";
    const char* const _key = "A key name of System.Windows.Forms.Keys (e.g. Enter, Tab, F5, A, D1, Left), an alias (Ctrl, Alt, Shift, Win, Esc, Backspace, 0-9), or a virtual key code number.";
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
    { .name = "Control.Wait", .description = "Waits before the next action." },
    {
      { &ControlWaitAction::Duration, "duration", { .is_required = true, .schema = { .description = "Milliseconds.", .minimum = 0 } } }
    });

  json_object_descriptor<ControlJoinAction> ControlJoinAction::json_description = describe_json_object<ControlJoinAction, Action>(
    { .name = "Control.Join", .description = "Waits for all async actions to complete." }, {});

  json_object_descriptor<ControlResetAction> ControlResetAction::json_description = describe_json_object<ControlResetAction, Action>(
    { .name = "Control.Reset", .description = "Cancels pending async actions and releases all held mouse buttons, keys and touch points. Does not move the cursor." }, {});
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
    { .name = "Mouse.Click", .description = "Clicks a mouse button; with x and y the cursor first moves there instantly." },
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
    { .name = "Mouse.Press", .description = "Presses a mouse button at the cursor and holds it until Mouse.Release." },
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
    { .name = "Capture.Screen", .description = "Captures the session's whole desktop." }, {});

  json_object_descriptor<CaptureWindowAction> CaptureWindowAction::json_description = describe_json_object<CaptureWindowAction, CaptureAction>(
    { .name = "Capture.Window", .description = "Captures one window; prefer it over Capture.Screen to save context." },
    {
      { &CaptureWindowAction::Window, "window", { .is_required = false, .schema = { .description = "A window ID from Window.Enumerate; default: the active window." } } }
    });

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
    { .name = "Window.Enumerate", .description = "Lists the open top-level windows: ID, title, process, position, size, state, and which one is in the foreground." }, {});

  json_object_descriptor<WindowFocusAction> WindowFocusAction::json_description = describe_json_object<WindowFocusAction, Action>(
    { .name = "Window.Focus", .description = "Brings a window to the foreground and gives it keyboard focus, restoring it if minimized. Use it before typing into a window." },
    {
      { &WindowFocusAction::Window, "window", { .is_required = true, .schema = { .description = _window } } }
    });

  json_object_descriptor<WindowMoveAction> WindowMoveAction::json_description = describe_json_object<WindowMoveAction, Action>(
    { .name = "Window.Move", .description = "Moves a window's top-left corner to (x, y)." },
    {
      { &WindowMoveAction::Window, "window", { .is_required = true, .schema = { .description = _window } } },
      { &WindowMoveAction::X, "x", { .is_required = true, .schema = { .description = _x } } },
      { &WindowMoveAction::Y, "y", { .is_required = true, .schema = { .description = _y } } }
    });

  json_object_descriptor<WindowResizeAction> WindowResizeAction::json_description = describe_json_object<WindowResizeAction, Action>(
    { .name = "Window.Resize", .description = "Resizes a window." },
    {
      { &WindowResizeAction::Window, "window", { .is_required = true, .schema = { .description = _window } } },
      { &WindowResizeAction::Width, "width", { .is_required = true, .schema = { .description = _width, .minimum = 1 } } },
      { &WindowResizeAction::Height, "height", { .is_required = true, .schema = { .description = _height, .minimum = 1 } } }
    });

  json_object_descriptor<WindowMinimizeAction> WindowMinimizeAction::json_description = describe_json_object<WindowMinimizeAction, Action>(
    { .name = "Window.Minimize", .description = "Minimizes a window." },
    {
      { &WindowMinimizeAction::Window, "window", { .is_required = true, .schema = { .description = _window } } }
    });

  json_object_descriptor<WindowMaximizeAction> WindowMaximizeAction::json_description = describe_json_object<WindowMaximizeAction, Action>(
    { .name = "Window.Maximize", .description = "Maximizes a window." },
    {
      { &WindowMaximizeAction::Window, "window", { .is_required = true, .schema = { .description = _window } } }
    });

  json_object_descriptor<WindowRestoreAction> WindowRestoreAction::json_description = describe_json_object<WindowRestoreAction, Action>(
    { .name = "Window.Restore", .description = "Restores a window to its previous size and position." },
    {
      { &WindowRestoreAction::Window, "window", { .is_required = true, .schema = { .description = _window } } }
    });

  json_object_descriptor<WindowCloseAction> WindowCloseAction::json_description = describe_json_object<WindowCloseAction, Action>(
    { .name = "Window.Close", .description = "Asks a window to close, as its X button does; the application may ask for confirmation first." },
    {
      { &WindowCloseAction::Window, "window", { .is_required = true, .schema = { .description = _window } } }
    });
#pragma endregion

#pragma region Process
  json_object_descriptor<ProcessRunAction> ProcessRunAction::json_description = describe_json_object<ProcessRunAction, Action>(
    { .name = "Process.Run", .description = "Runs a command in the session. Synchronous by default: waits for the exit and returns the exit code and the output and error streams (each truncated to 64 KB). Use async for programs that keep running, e.g. GUI applications or a console to interact with: it starts the program as Explorer would and returns its process ID at once; with shell None the ID is the program's own." },
    {
      { &ProcessRunAction::Command, "command", { .is_required = true, .schema = { .description = "The command for the shell, or with shell None the path of an executable." } } },
      { &ProcessRunAction::Shell, "shell", { .is_required = false, .schema = { .description = "PowerShell (default, Windows PowerShell), Pwsh (PowerShell 7), Cmd, or None (no shell parsing)." } } },
      { &ProcessRunAction::Args, "args", { .is_required = false, .schema = { .description = "Arguments, passed without shell parsing with shell None." } } },
      { &ProcessRunAction::WorkingDirectory, "workingDirectory", { .is_required = false, .schema = { .description = "Default: the user's profile folder." } } },
      { &ProcessRunAction::Async, "async", { .is_required = false, .schema = { .description = "Start the program as Explorer would (console programs get a window, nothing is redirected) and return its process ID at once; Control.Join does not wait for it, Process.WaitForProcess does." } } },
      { &ProcessRunAction::Timeout, "timeout", { .is_required = false, .schema = { .description = "Milliseconds to wait for the exit, default 10000; when it expires the sequence stops with a Timeout error and the process keeps running.", .minimum = 0 } } }
    });

  optional<milliseconds> ProcessRunAction::WaitTimeout() const
  {
    return ToMilliseconds(Timeout);
  }

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
      { &ProcessResult::IsErrorTruncated, "stderrTruncated", { .is_required = false } }
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
