#pragma once
#include "CommonApi.h"

//Named enums of the actions and their results. They are written as JSON strings and parsed case-insensitively.
namespace SecondShell::Actions
{
  named_enum(ActionKind,
    None,
    Control,
    Mouse,
    Keyboard,
    Touch,
    Pen,
    Capture,
    Window,
    Process,
    Clipboard
  );

  named_enum(ActionType,
    None,
    ControlWait,
    ControlJoin,
    ControlReset,
    MouseMove,
    MouseClick,
    MouseDoubleClick,
    MouseWheel,
    MousePress,
    MouseRelease,
    MouseReset,
    TouchPress,
    TouchRelease,
    TouchTap,
    TouchMove,
    TouchReset,
    PenMove,
    PenPress,
    PenRelease,
    PenTap,
    PenReset,
    KeyboardType,
    KeyboardTap,
    KeyboardPress,
    KeyboardRelease,
    KeyboardReset,
    CaptureScreen,
    CaptureWindow,
    CaptureRegion,
    CaptureWaitForText,
    CaptureWaitForStable,
    WindowEnumerate,
    WindowFocus,
    WindowMove,
    WindowResize,
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    WindowClose,
    ProcessRun,
    ProcessStop,
    ProcessKill,
    ProcessWaitForProcess,
    ProcessEnumerate,
    ClipboardRead,
    ClipboardWrite
  );

  named_enum(ActionStatus,
    Succeeded,
    Failed,
    Cancelled
  );

  named_enum(ErrorCode,
    SessionNotActive,
    UserInterrupted,
    AgentInputDisabled,
    Timeout,
    InvalidArgument,
    ActionFailed,
    SetupRequired
  );

  named_enum(MouseButton,
    Left,
    Right,
    Middle
  );

  named_enum(WheelAxis,
    Vertical,
    Horizontal
  );

  named_enum(KeyModifier,
    Shift,
    Ctrl,
    Alt,
    Win
  );

  named_enum(CaptureOutput,
    Inline,
    File,
    Both
  );

  named_enum(ImageFormat,
    Png,
    Jpeg
  );

  named_enum(ShellKind,
    PowerShell,
    Pwsh,
    Cmd,
    None
  );

  named_enum(ClipboardFormat,
    Text,
    Html,
    Image,
    Files,
    Raw
  );

  named_enum(WindowState,
    Normal,
    Minimized,
    Maximized
  );
}
