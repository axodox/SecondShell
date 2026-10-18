# SecondShell

Second shell is a secondary interactive shell for Windows that allows AI agents to work with the desktop environment without disturbing the user.

## Features

- Screen capture: allows the AI agent to capture the current state of the desktop environment.

- Display management: allows the AI to resize the display, change the display resolution, and query display information.

- Input emulation: allows the AI agent to simulate user input, such as mouse clicks, single and multi-touch input, gamepad input and keyboard presses.

- Clipboard management: allows the AI agent to read from and write to the clipboard.

- Window management: allows the AI agent to manage windows, such as moving, resizing, minimizing, maximizing, and closing windows.

- Process management: allows the AI agent to start and stop processes, and wait for processes to complete.

- Observability: provides visual feedback to the user about input sequences and screen captures performed by the AI agent. It also provides a log of all actions performed by the AI agent.

- Manual input: allows the user to manually provide input to the second session, such as mouse clicks and keyboard presses, touch and gamepad input.

To provide good performance multiple inputs and screen captures may be performed in a single tool call. The AI agent can request an action sequence consisting of various inputs and screen captures with timing information.

## Architecture

SecondShell is built on two Windows features (see Findings.md): **child sessions**, a second session of the same user on the same machine, and the in-box **Remote Desktop ActiveX control** (`mstscax.dll`), which connects to the child session over a local loopback.

The solution consists of three executables and a shared library. Both builds (portable and MSIX) ship the same binaries.

| Component | Runs in | Purpose |
|---|---|---|
| `SecondShell.exe` | User's session | The host: the SecondShell window. Owns the RDP connection (in a separate hidden window, see Session display), signs in, manages the session lifecycle and display settings, shows the session using frames from the driver, draws the visualization overlay, and enforces read-only mode and the Stop button. Even without AI, the user can use it to work in the second session. Single instance. Also performs machine-wide setup when started elevated with `--setup` (see Setup and repair). |
| `SecondShell.Mcp.exe` | User's session | stdio MCP server started by the AI client. Translates tool calls into requests to the host. Short-lived: when the AI client restarts it, it reconnects to the running host, and the session is unaffected. Starts the host if it isn't running. |
| `SecondShell.Driver.exe` | Child session | No visible UI. Performs all actions inside the session: input, capture, windows, processes, clipboard, OCR. It has a hidden message window for clipboard monitoring, and is per-monitor DPI-aware (v2) so that its coordinates are session pixels. |
| `SecondShell.Common.dll` | (all) | Shared code: the pipe protocol, data structures, and the build-specific settings (see Deployment). |

```
AI client ──stdio──► SecondShell.Mcp.exe ──pipe──► SecondShell.exe (host window) ──RDP──► child session
                                                          ▲                                   │
                                                          └──────────────pipe───── SecondShell.Driver.exe
```

**All requests go through the host.** The MCP server never talks to the driver directly, so the host sees every action before it is executed. This is what makes the overlay complete and the Stop button reliable.

### Working without focus

SecondShell must work while the user is working in other applications. It must never need its window to be in the foreground, and never take focus from the user. All agent actions are performed by the driver inside the child session, which doesn't depend on the host window's state (verified for capture and `SendInput` with the RDP window hidden, see Findings.md 2.5). The driver itself is started by Windows as a startup app, so starting it doesn't need focus either (see Starting the driver).

### Session display

The RDP control only keeps the session alive; it is not what the user sees.

- The RDP control lives in its **own hidden window**, separate from the visible SecondShell window. Its window is never shown and never minimized. Minimizing the RDP window stops the session from drawing, so captures fail (Findings.md 2.5), but hiding it or moving it off-screen does not.
- The **driver captures the session** (`Windows.Graphics.Capture`) and passes the frames to the host. Within the GPU this uses shared textures; the host draws them into its own DirectX swap chain.
- The host draws the **visualization overlay** into the same swap chain, so there is no separate overlay window and no airspace problem between the RDP control and the UI.
- The visible SecondShell window can be minimized, resized or moved freely without affecting the session.
- In interactive mode, the user's mouse and keyboard input in the SecondShell window is forwarded to the driver, which injects it with `SendInput`. The RDP control's own input handling is not used.

To verify: frame transport between sessions with `Windows.Graphics.Capture` and shared textures (the tests so far used GDI captures written to files), and the latency of interactive mode.

### Session lifecycle

- **Start**: the host connects to the child session. If a disconnected child session already exists (e.g. after a host crash), the connection resumes it with its apps still running. The host reports this, and the user or `Initialize-Session` chooses whether to keep it or reset it.
- **End**: disconnecting does not reliably end a child session; it can stay alive, disconnected, indefinitely (Findings.md 2.3). The host therefore always logs the session off explicitly (`WTSLogoffSession`, no elevation needed) when the window is closed, on `Shutdown-Session`, and when resetting.
- **Startup apps**: the child session is a full logon of the same user, so the user's startup apps run in it a second time (e.g. sync clients, media servers, torrent clients). This is accepted and documented. Apps that conflict with themselves (fixed ports, exclusive data files) are the user's to configure.
- **Shared app state**: apps started in the child session see the user's own app state, and may write to it. For example, Notepad restores the user's open tabs (including unsaved ones) in the child session and saves window state back to the shared profile (Findings.md 2.10). The agent must treat such apps as working on the user's real data.

### Starting the driver

A process in the user's session cannot start a process in another session without SYSTEM privileges. Instead, the host lets Windows start the driver at the child session's logon, using a **`RunOnce` entry**: Windows runs it at the user's next logon and deletes it. The user's own session is already logged on, so the next logon of the user is the child session's. No elevation, keystrokes or focus are needed (verified: started 12 s after sign-in with the RDP window hidden, and the entry was consumed, Findings.md 2.8).

1. Just before connecting to a **new** child session, the host writes the per-user `RunOnce` entry `HKCU\Software\Microsoft\Windows\CurrentVersion\RunOnce\SecondShellDriver` = the absolute path of `SecondShell.Driver.exe`.
2. Windows starts the driver during the child session's logon and deletes the entry.
3. The host waits for the driver to connect to its pipe and pass the session guard. If it doesn't within the timeout of `Initialize-Session` (default 3 minutes, as the driver starts with the user's startup programs, which can take more than a minute), the host reports an error.
4. If the connection fails before logon, or **resumes an existing session** (no new logon happens, and the driver started at that session's logon is normally still running), the host deletes the unconsumed entry.

Compared with a permanent `Run` entry (also verified, Findings.md 2.8), `RunOnce` doesn't start the driver at the user's normal logons, doesn't appear in Task Manager's startup list, and can't be disabled there by accident. If the host crashes between writing the entry and the logon, the entry runs at the user's next normal logon instead; the session guard makes the driver exit immediately there, and the host deletes any leftover entry on its next start.

- **Driver crash**: the host resets the session (log off, write the entry, connect again), and the driver starts at the new logon. This loses the session's state, which is acceptable for a crash.
- **MSIX build**: HKCU writes from a packaged app are redirected into the package (see Deployment), so Explorer may not see the entry. *To verify*; the fallbacks are writing it from a process outside the package context, or a startup task declared in the manifest (which would behave like a permanent `Run` entry).

Two other methods were examined and are not used (Findings.md 2.6, 2.7):

- Typing the launch command (Win+R) into the session through the RDP control works, but only while the RDP window is active, which conflicts with Working without focus.
- COM activation in the child session (`Session:<id>!clsid:{...}`, as the Windows Simulator does) starts the server in the caller's session when it is registered per user. It probably needs a machine-wide registration, i.e. elevation.

### Session guard

Every SecondShell program that runs in the child session (the driver) must make sure, before doing anything else, that it is running in the child session the host expects:

1. On start, the driver connects to the host's pipe. If there is no host pipe, e.g. at a normal logon of the user's own session, it exits immediately.
2. The host checks the client's session ID (`GetNamedPipeClientSessionId`) against the current child session ID. If they differ, it tells the driver to exit, and the driver exits immediately.
3. Only after the host has accepted it does the driver touch input, the screen or the clipboard.

This prevents a misdirected start from moving the user's real mouse or showing windows on the user's desktop. That happened once in testing, with COM starting a test program in the user's session (Findings.md 2.7). The same guard, based on a file instead of the pipe, was verified with test programs started both by COM and as a startup app.
## MCP tools

- `Initialize-Session`: initializes a new session and starts the second shell process. Display settings and capture settings (see `Configure-Capture`) can be specified, otherwise the defaults will be used. Waits until the login is complete, or until its timeout expires (default 3 minutes).

- `Check-Session`: checks the status of the current session and returns information about the session, such as whether it is active, the display settings, and the process ID of the second shell process. Normally it can be assumed after initialization that the session is active, and running a sequence will check the session status anyways and return an error if the session is not active.

- `Shutdown-Session`: closes the current session and terminates the second shell process.

- `Run-Sequence`: performs a sequence of actions, this is the main tool used by the AI agent to perform input simulation and screen capture.

- `Set-Display`: sets the display resolution and other display settings (resolution, scaling, orientation); settings left out stay as they are. Returns once captures show the new size.

- `Get-Display`: gets the current display settings and information.

- `Configure-Capture`: sets the session's capture defaults: the capture resolution (see `"resolution"` under Capture output), the default format, and the maximum number of inline captures per sequence. Capture actions may override the resolution and format individually.

All tools that wait for something have a timeout with a sensible default, which the caller may override. When a timeout expires the tool returns a `Timeout` error (see Errors and timeouts).

## Input simulation

### Basics

Inputs are defined as a sequence of actions, which are executed in order. Each action is represented as a JSON object with an `"action"` field specifying the type of action, and additional fields specifying the parameters for that action. Action names and enum values (e.g. `"Left"`, `"inline"`) are case-insensitive; the tool schema lists enum values in PascalCase. Unknown properties are rejected, so typos are reported instead of ignored.

Actions are normally executed sequentially. Most actions require no duration setting, instead a default duration is used, so the system may register the input and process it. 

Some actions, such as mouse movement, may require a duration setting to specify how long the action should take. The duration is specified in milliseconds. For now the animated value is linear, but in the future it may be possible to specify different easing functions.

Any action which has a duration setting may be executed asynchronously, by specifying `"async": true` in the action object. In this case the action will be executed in parallel with the next actions until it is completed.

### Input injection

The driver injects all input inside the child session with **`Windows.UI.Input.Preview.Injection.InputInjector`** (WinRT). One API covers every device SecondShell supports, and applications receive the input through the same pipeline as real input:

| Device | `InputInjector` methods | Status |
|---|---|---|
| Keyboard | `InjectKeyboardInput` | Verified unpackaged in the child session (Findings.md 2.9): Unicode text (`InjectedInputKeyOptions.Unicode`, layout-independent, typed `ÁÉŐŰ` correctly into Notepad) and virtual keys (Ctrl+S, Ctrl+W). |
| Mouse | `InjectMouseInput` | Verified: absolute move in session pixels, all buttons and both wheels (Findings.md 2.14). |
| Touch | `InitializeTouchInjection`, `InjectTouchInput`, `UninitializeTouchInjection` | Verified in the child session: tap, drag, multi-touch, as `WM_POINTER` touch input (Findings.md 2.14). The contact rectangle is in absolute screen pixels. |
| Pen | `InitializePenInjection`, `InjectPenInput`, `UninitializePenInjection` | Verified in the child session: hover, pressure, tilt, rotation, barrel button, eraser (`Inverted` + `Eraser`) (Findings.md 2.14). |
| Gamepad | `InitializeGamepadInjection`, `InjectGamepadInput`, `UninitializeGamepadInjection` | **Deferred.** From an unpackaged process the injected controller is not seen by Windows.Gaming.Input, XInput or GameInput (Findings.md 2.15). Options for later: a packaged build with `inputInjectionBrokered`, or a virtual controller driver (elevation to install). The gamepad actions below are kept as the target design. |

`InputInjector.TryCreate()` works unpackaged, although the API is documented as needing the restricted capability `inputInjectionBrokered`, and it is still in the `Preview` namespace. Both could change in a future Windows version. The driver therefore keeps its injection code behind one internal interface, with the Win32 APIs as a fallback if `TryCreate()` ever fails:

- mouse and keyboard: `SendInput` (also verified in the child session, Findings.md 2.5);
- touch and pen: `CreateSyntheticPointerDevice` + `InjectSyntheticPointerInput` (Windows 10 1809+);
- gamepad: no Win32 equivalent.

Limits that apply to all injected input:

- **Elevated applications**: Windows blocks injected input from a non-elevated process to windows of elevated processes (UIPI). The agent can therefore not operate applications running as administrator, nor UAC prompts (which appear on the secure desktop). Lifting this would need a signed `uiAccess` build installed under Program Files, which conflicts with the portable build; not planned.
- **Detectable**: injected mouse and keyboard input is marked as injected. A few applications (e.g. games with anti-cheat) detect or ignore it.

### Coordinates

All coordinates, sizes and regions in actions and results are in **session pixels**: physical pixels of the child session's desktop, with (0, 0) at its top-left corner. This applies regardless of the session's DPI scaling, the size of the SecondShell window, and the resolution of captures. When a capture is returned at a lower resolution than the session, its result reports the scale factor, so positions seen in the image can be converted back to session pixels.

### Errors and timeouts

- **Run until failure**: `Run-Sequence` executes actions in order until one fails, then stops. The result contains the results of all completed actions (including captures already taken), the index of the failed action, an error code and a message.

- **Input state after failure**: when a sequence stops early, all inputs are released as by `Control.Reset` (held mouse buttons, keys, touch points and gamepad buttons; gamepad sticks and triggers return to rest), and the result reports what was held, so the agent knows the state it left the session in.

- **User interruption**: if the user presses "Stop" or switches the window to interactive mode, the running sequence stops with a `UserInterrupted` error. Until the user gives control back, all further sequences fail immediately with `AgentInputDisabled`.

- **Timeouts**:
  - `Run-Sequence` accepts a master `"timeout"` for the whole sequence (default 60 seconds).
  - Every action that waits for something (`Process.WaitForProcess`, `Capture.WaitForText`, synchronous `Process.Run`, etc.) accepts its own `"timeout"` (default 10 seconds).
  - Session tools (`Initialize-Session`, `Shutdown-Session`, etc.) have default timeouts too.
  - Whichever timeout expires first stops the sequence with a `Timeout` error.

- **Error codes**: `SessionNotActive`, `UserInterrupted`, `AgentInputDisabled`, `Timeout`, `InvalidArgument` (e.g. unknown window ID or coordinates outside the screen), `ActionFailed` (the action itself failed; the message has the details).

### Control

- `{ "action": "Control.Wait", "duration": durationMS }`: **Wait**: waits for the specified duration in milliseconds before executing the next action.

- `{ "action": "Control.Join" }`: **Join**: waits for all asynchronous actions to complete before executing the next action.

- `{ "action": "Control.Reset" }`: **Reset**: cancels any pending asynchronous actions and releases all inputs on all devices: mouse buttons, keys, touch points, gamepad buttons, and gamepad sticks and triggers back to their rest positions. It does not move the mouse cursor. Equivalent to running every device's `Reset` action.

### Process management

- `{ "action": "Process.Run", "command": command, "shell": "PowerShell/Pwsh/Cmd/None", "args": [arg], "workingDirectory": path, "async": true/false, "timeout": timeoutMS }`: **Run**: executes a command inside the child session.
  - `shell` selects how `command` is interpreted: `PowerShell` (default, Windows PowerShell), `Pwsh` (PowerShell 7, if installed), `Cmd`, or `None`. With `None`, `command` is the path of an executable and `args` its arguments, passed without any shell parsing. The default shell can be changed in the SecondShell settings.
  - `workingDirectory` is optional; default is the user's profile folder.
  - Synchronous (default): waits for the process to exit, then returns its exit code and its output and error streams. Each stream is truncated to 64 KB, and the result says if it was. If `timeout` (default 10 seconds) expires, the sequence stops with a `Timeout` error; the process keeps running and its PID is reported, so it can be stopped or killed.
  - Synchronous runs have no window and an empty input stream, so they suit commands, not programs to interact with.
  - Asynchronous (`"async": true`): starts the program as Explorer would and returns its PID immediately; it keeps running in parallel with the next actions, and `Control.Join` does not wait for it. Console programs get a window of their own, and nothing is redirected, so the output is not collected. Use this for GUI applications, which otherwise block until they are closed, and for consoles to interact with. With `shell` `None` the PID is the program's own; with a shell it is the shell's. `Process.WaitForProcess` waits for it later.

- `{ "action": "Process.Stop", "pid": pid, "timeout": timeoutMS }`: **Stop process**: asks the process to exit gracefully, like a user would: its windows receive a close request (as when clicking X), and console processes receive Ctrl+C. The application may still ask for confirmation (e.g. to save changes). Waits up to `timeout` (default 10 seconds) for the process to exit and returns its exit code. If it hasn't exited by then, the sequence stops with a `Timeout` error; use `Process.Kill` to force it.

- `{ "action": "Process.Kill", "pid": pid, "tree": true/false }`: **Kill process**: terminates the process immediately, without giving it a chance to save or clean up. With `"tree": true` its child processes are terminated as well (default false). Returns once the process has exited.

- `{ "action": "Process.WaitForProcess", "pid": pid, "timeout": timeoutMS }`: **Wait for process**: waits for the specified process to complete before executing the next action, and returns its exit code. The PID of the process can be obtained from a previous `Process.Run` action. The timeout is the maximum time to wait for the process to complete, default is 10 seconds.

- `{ "action": "Process.Enumerate" }`: **Enumerate processes**: returns a list of all running processes with their PIDs, names, and other relevant information, such as CPU/GPU/RAM/disk/network utilization.

### Screen capture

#### Capture output

All capture actions share these optional fields:

- `"tag"`: a label for the capture, returned with it in the result. Default: the index of the capture in the current sequence.

- `"output"`: where the capture goes:
  - `"inline"` (default): returned as MCP image content in the `Run-Sequence` result. This is the only form the model can see directly, so it is the default for the usual act-then-look loop.
  - `"file"`: saved to disk. The result contains the file path and the image dimensions instead of the image. Use for evidence, baselines, and high-volume captures (e.g. animations) that would fill the agent's context if returned inline.
  - `"both"`: returned inline and saved to disk.

- `"path"`: the file path for `"file"` / `"both"`. Default: `%LOCALAPPDATA%\SecondShell\Captures\<session>\<sequence>-<step>-<tag>.<ext>`. Old session folders are cleaned up automatically. The child session shares the file system with the user's session, so saved paths are also valid for the AI client's own file tools.

- `"format"`: `"png"` (default) or `"jpeg"`. Inline captures are always PNG: with Claude, an image's token cost depends on its pixel size, not its file size, and PNG keeps text sharp. JPEG is only useful to save disk space for files. The default can be changed with `Configure-Capture`.

- `"resolution"`: `{ "width": w, "height": h }`, the maximum size of the capture. The captured image is downscaled to fit within it, keeping its aspect ratio, and is never upscaled. The default comes from `Initialize-Session` / `Configure-Capture`; if neither sets it, captures are at full session resolution. The result reports the resulting scale factor (see Coordinates). To save context, prefer capturing a smaller area (`Capture.Window`, `Capture.Region`) over a lower resolution, which makes text harder to read.

A sequence returns at most 4 inline captures (configurable). Further captures marked `"inline"` are saved to file instead, and their result says so. This stops a runaway sequence from flooding the agent's context.

#### Capture actions

- `{ "action": "Capture.Screen" }`: **Capture screen**: captures the whole desktop of the session.

- `{ "action": "Capture.Window", "window": windowId }`: **Capture window**: captures a single window. `window` is optional; if not set, the active window is captured.

- `{ "action": "Capture.Region", "x": x, "y": y, "width": width, "height": height }`: **Capture region**: captures the specified rectangle of the screen.

- `{ "action": "Capture.WaitForText", "text": text, "interval": intervalMS, "timeout": timeoutMS }`: **Wait for text**: waits for the specified text to appear on the screen before executing the next action. The text is specified as a string. The interval is the time in milliseconds between checks, default is 500 milliseconds. The timeout is the maximum time to wait for the text to appear, default is 10 seconds. This relies on OCR to detect the text on the screen, so it may not be 100% accurate.

- `{ "action": "Capture.WaitForStable", "stable": stableMS, "window": windowId, "interval": intervalMS, "timeout": timeoutMS }`: **Wait for stable screen**: waits until the screen stops changing, i.e. until it has stayed the same for `stable` milliseconds (default 500). Use it after an action whose effect takes an unknown time, such as opening a window or loading a page, instead of a fixed `Control.Wait`. `window` is optional and limits the check to one window; alternatively `x`, `y`, `width` and `height` limit it to a region. Defaults: `interval` 100 milliseconds, `timeout` 10 seconds. Ongoing animations (e.g. a blinking caret or a spinner) can keep the screen from ever becoming stable, so in that case limit the check to the relevant window or region.

### Mouse input

Actions:

- `{ "action": "Mouse.Move", "x": x, "y": y, "duration": durationMS }`: **Move to (x, y)**: moves the mouse cursor to the specified screen coordinates.

- `{ "action": "Mouse.Click", "button": "Left/Right/Middle", "x": x, "y": y }`: **Click (left, right, middle)**: simulates a mouse click. If `x` and `y` are specified, the cursor first moves there instantly; otherwise the click happens at the current cursor position. Duration can be specified to simulate a long press, but optional.

- `{ "action": "Mouse.DoubleClick", "button": "Left/Right/Middle", "x": x, "y": y }`: **Double click**: simulates a double click, by default with the left button. `x` and `y` are optional, as for `Mouse.Click`.

- `{ "action": "Mouse.Wheel", "delta": notches, "axis": "Vertical/Horizontal", "x": x, "y": y }`: **Wheel**: rotates the mouse wheel by the given number of notches (may be fractional for high-resolution scrolling). For the vertical axis positive scrolls up, for the horizontal axis positive scrolls right. `axis` defaults to vertical. `x` and `y` are optional; if specified, the cursor first moves there, since the wheel scrolls the element under the cursor.

- `{ "action": "Mouse.Press", "button": "Left/Right/Middle" }`: **Press (left, right, middle)**: simulates pressing a mouse button at the current cursor position. The mouse button is held down until a release action is received.

- `{ "action": "Mouse.Release", "button": "Left/Right/Middle" }`: **Release (left, right, middle)**: simulates releasing a mouse button at the current cursor position.

- `{ "action": "Mouse.Reset" }`: **Reset**: releases all mouse buttons. The cursor stays where it is.

### Touch input

- `{ "action": "Touch.Press", "id": touchId, "x": x, "y": y }`: **Start touch**: puts a finger down at the specified screen coordinates. The touch identifier must not be in use by another active touch point.

- `{ "action": "Touch.Release", "id": touchId }`: **End touch**: lifts the finger with the specified touch identifier, at its current position.

- `{ "action": "Touch.Tap", "id": touchId, "x": x, "y": y }`: **Tap**: simulates a quick touch press and release at the specified screen coordinates with a unique touch identifier.

- `{ "action": "Touch.Move", "id": touchId, "x": x, "y": y, "duration": durationMS }`: **Move touch**: simulates a touch move event for the specified touch identifier to the new screen coordinates.

- `{ "action": "Touch.Reset" }`: **Reset**: lifts all active touch points.

### Keyboard input

**Key names** are the member names of the .NET `System.Windows.Forms.Keys` enumeration, case-insensitive, for example `Enter`, `Escape`, `Tab`, `Back`, `Delete`, `F5`, `A`, `D1` (the 1 key above the letters), `NumPad1`, `Left`, `PageDown`, `LWin`. Its values are the Windows virtual key codes, so a numeric virtual key code is accepted as well. Common aliases are also accepted, because models tend to use them: `Ctrl` (= `ControlKey`), `Alt` (= `Menu`), `Shift` (= `ShiftKey`), `Win` (= `LWin`), `Esc`, `Backspace`, and `0`–`9` for `D0`–`D9`.

**Modifiers** are `Shift`, `Ctrl`, `Alt` and `Win`.

Keys that cannot be simulated: Ctrl+Alt+Del is handled by Windows itself and can't be injected. Win+L locks the child session, and simulated input can't unlock it.

- `{ "action": "Keyboard.Type", "text": text }`: **Type text**: simulates typing the specified text string. The text is typed as Unicode characters, so it doesn't depend on the session's keyboard layout and may contain any character. A line break (`\n`) is typed as Enter and a tab (`\t`) as Tab; use `Keyboard.Tap` for other special keys. `"pause": durationMS` can be specified to add a delay between keystrokes otherwise a default delay is used. `"sensitive": true` hides the typed text from the action log and the on-screen visualization (e.g. for passwords).

- `{ "action": "Keyboard.Tap", "key": key, "modifiers": [modifier] }`: **Tap key**: simulates pressing and releasing a specific key (see Key names). Modifiers can be specified to simulate holding down modifier keys (e.g., Shift, Ctrl, Alt) while pressing the key, e.g. `{ "action": "Keyboard.Tap", "key": "S", "modifiers": ["Ctrl"] }` for Ctrl+S.

- `{ "action": "Keyboard.Press", "key": key, "modifiers": [modifier] }`: **Press key**: presses the modifiers, then the key, and holds them down until they are released.

- `{ "action": "Keyboard.Release", "key": key, "modifiers": [modifier] }`: **Release key**: releases the key, then the modifiers.

- `{ "action": "Keyboard.Reset" }`: **Reset**: releases all pressed keys and resets the keyboard state.

### Gamepad input

The simulated gamepad is an Xbox controller. Names follow `Windows.Gaming.Input`:

- **Buttons**: `A`, `B`, `X`, `Y`, `LeftShoulder`, `RightShoulder`, `View`, `Menu`, `LeftThumbstick`, `RightThumbstick` (pressing the stick), `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`.
- **Axes**: `LeftThumbstickX`, `LeftThumbstickY`, `RightThumbstickX`, `RightThumbstickY` range from -1.0 to 1.0 (right and up are positive), rest position 0. `LeftTrigger` and `RightTrigger` range from 0.0 to 1.0, rest position 0.

Actions:

- `{ "action": "Gamepad.ButtonPress", "button": button }`: **Press button**: presses a button and holds it down until it is released.

- `{ "action": "Gamepad.ButtonRelease", "button": button }`: **Release button**: releases a button.

- `{ "action": "Gamepad.ButtonTap", "button": button }`: **Tap button**: simulates a quick press and release of a button.

- `{ "action": "Gamepad.MoveAxis", "axis": axis, "value": value, "duration": durationMS }`: **Move axis**: moves a stick axis or trigger to the given value, over `duration` milliseconds if specified (otherwise instantly). The axis stays there until moved again or reset.

- `{ "action": "Gamepad.Reset" }`: **Reset**: releases all buttons and returns all sticks and triggers to their rest positions.

### Window management

**Window IDs** are window handles (HWND) written as hexadecimal strings, e.g. `"0x0001A2B4"`. They are valid only inside the child session and only while the window exists. Using the ID of a window that has closed gives an `InvalidArgument` error.

- `{ "action": "Window.Enumerate" }`: **Enumerate windows**: returns a list of all open top-level windows with their ID, title, process ID and name, position and size, state (normal, minimized, maximized), and which one is in the foreground.

- `{ "action": "Window.Focus", "window": windowId }`: **Focus window**: brings the window to the foreground and gives it keyboard focus, restoring it first if it is minimized. Use it before typing into a window. Returns an error if Windows refused to switch the foreground window.

- `{ "action": "Window.Move", "window": windowId, "x": x, "y": y }`: **Move window**: moves the specified window to the specified screen coordinates.

- `{ "action": "Window.Resize", "window": windowId, "width": width, "height": height }`: **Resize window**: resizes the specified window to the specified width and height.

- `{ "action": "Window.Minimize", "window": windowId }`: **Minimize window**: minimizes the specified window.

- `{ "action": "Window.Maximize", "window": windowId }`: **Maximize window**: maximizes the specified window.

- `{ "action": "Window.Restore", "window": windowId }`: **Restore window**: restores the specified window to its previous size and position.

- `{ "action": "Window.Close", "window": windowId }`: **Close window**: asks the window to close, as when clicking its X button. The application may ask for confirmation first.

### Clipboard management

Clipboard content is described by a **format** and its **data**:

| Format | Data | Windows clipboard format |
|---|---|---|
| `text` | string | `CF_UNICODETEXT` |
| `html` | string, an HTML fragment | `HTML Format` (SecondShell adds the required header) |
| `image` | base64-encoded PNG, or `"path"` to an image file instead of `"data"` | `CF_DIBV5` and `PNG` |
| `files` | array of file paths (valid in the session, which shares the user's file system) | `CF_HDROP` |
| `raw` | base64-encoded bytes, plus `"name"`: a registered clipboard format name or a standard format number | as named |

- `{ "action": "Clipboard.Read", "format": format }`: **Read clipboard**: reads the clipboard in the requested format, and lists the formats currently on it. Without `format`, returns the text too, if there is any. For `raw`, `"name"` gives the clipboard format, as for writing. Images are returned like captures: inline by default, and the capture output fields (`output`, `path`, `resolution`) apply. Raw data is limited to 1 MB.

- `{ "action": "Clipboard.Write", "content": [{ "format": format, "data": data }] }`: **Write clipboard**: replaces the clipboard content. `content` is a list, so the same content can be offered in several formats at once (e.g. `text` and `html`), as applications usually do.

Both actions work on the child session's own clipboard. Every Windows session has its own clipboard, and SecondShell turns off RDP clipboard redirection, so by default the agent's clipboard and the user's clipboard are completely separate: the agent never overwrites what the user copied, and never sees it.

When the user switches the window to interactive mode, SecondShell syncs the two clipboards in both directions, so the user can copy and paste between their desktop and the session. Leaving interactive mode stops syncing. Interactive mode is off by default. Syncing is implemented by SecondShell itself (watching the clipboard on both sides and copying changes across), not by RDP clipboard redirection: as far as we know, RDP redirection settings apply only when connecting, and reconnecting would end the child session (see Findings.md).

## Visualization

The window's look, its status bar, log and dialogs are in UIDesign.md.

The SecondShell GUI provides visual feedback to the user about the actions performed by the AI agent. All visualization is an overlay drawn by the SecondShell window on top of its view of the session. Nothing is drawn inside the child session, so the visualization never appears in captures and never affects the applications being tested.

This includes:

- Displaying the desktop in a window, by default the desktop is displayed in a read-only mode, but the user can switch to an interactive mode to provide manual input. This is to avoid accidental input from the user while the AI agent is performing actions. The user can also interrupt the AI agent at any time by clicking a "Stop" button, which will reset all input devices and stop any ongoing actions until the user gives back control.

- The agents input is visualized on the screen:

  - Mouse/touch input is visualized with a cursor and click animations. For the mouse the buttons and scroll wheel are also visualized. For touch input, the touch points are visualized with circles that expand and fade out when released.

  - Keyboard input is visualized with on-screen keys that highlight when pressed. Keystrokes are shown for a short duration in the bottom left corner of the screen, then they scroll up and fade out, as more input is added.

  - Gamepad input is visualized with on-screen buttons that highlight when pressed. The axes and sticks are visualized with on-screen indicators that move according to the input values.

  - Screen captures are visualized with a yellow flashing border which fades out after a short duration.

  - Issued commands are visualized with a small overlay showing the process name and PID, which scrolls up and fades out after a short duration. In the same view where the keyboard, gamepad input is shown.

  - Clipboard read/write actions are visualized with a small overlay showing the content of the clipboard, which scrolls up and fades out after a short duration. In the same view where the keyboard, gamepad input is shown.

## Security

### Threat model

The child session runs as the same Windows user as the user's own desktop. Anything running in it, including commands started by the agent, has the same access to the user's files, registry, network and stored secrets. SecondShell isolates **input, display and clipboard** from the user's desktop, not **data**.

The measures below protect against accidental exposure and against other users and machines. They cannot fully protect against a deliberately malicious agent running as the same user. Users must only connect agents they would trust with their own account.

### Credentials

The child session needs the user's Windows password to sign in, because single sign-on does not work on every machine (see Findings.md).

- **Storage**: the password is stored in Windows Credential Manager as a generic credential named `SecondShell:ChildSession`. The stored blob is additionally encrypted with DPAPI (`CryptProtectData`, user scope) using an application-specific entropy value, so a plain Credential Manager read, by a script or a tool such as `cmdkey`, does not yield the password.
- **Target name**: never `termsrv/*`, `localhost` or similar RDP target names. Those change how RDP signs in to the child session, and the Windows Simulator refuses to run when they exist.
- **First sign-in**: the password is requested with the standard Windows credential prompt (`CredUIPromptForWindowsCredentials`), shown by the SecondShell window in the foreground. It is saved only after the sign-in has succeeded.
- **Rejected password**: if a stored password is rejected (e.g. after a password change), it is deleted and the user is prompted again.
- **Handling**: only `SecondShell.exe` ever handles the password. It is never passed to the MCP server or into the child session, never written to logs or command lines, and is kept in memory only for the duration of the connection attempt, then cleared.
- **Removal**: the SecondShell window has a "Forget saved password" command.

### Inter-process communication

- All communication uses local named pipes, created with `PIPE_REJECT_REMOTE_CLIENTS` and an access list that allows only the current user.
- `SecondShell.exe` accepts in-session connections only from processes whose session ID (`GetNamedPipeClientSessionId`) is the current child session.
- No network ports are opened.

### Privileges

- `SecondShell.exe`, `SecondShell.Mcp.exe` and everything in the child session run without elevation.
- Enabling child sessions is a separate one-time setup step that requests elevation (UAC) on its own (e.g. `SecondShell.exe --setup`).
- Commands started by the agent cannot elevate unattended: UAC prompts appear on the secure desktop of the child session, which simulated input cannot reach. This is intended.

### User control

- The window starts in read-only mode, so the user cannot accidentally interfere with the agent.
- The "Stop" button acts immediately: SecondShell stops forwarding agent actions, discards queued input, releases all held inputs, and keeps rejecting agent input until the user gives control back (see Errors and timeouts).
- Every agent action is written to the action log. Text typed with `"sensitive": true` is redacted. Clipboard contents are truncated in the log and in overlays.

### Files

Captures and logs are stored under `%LOCALAPPDATA%\SecondShell`, which is accessible only to the user. Old session folders are deleted automatically.

## Setup and repair

Normal use never needs elevation. Machine-wide setup needs it once per machine, and again only if that setup breaks or the app is moved.

### Checks on every start

`SecondShell.exe` checks, without elevation:

- **Machine-wide**: child sessions are enabled (`WTSIsChildSessionsEnabled`). This is the only required step that needs elevation.
- **Machine-wide, optional**: the smooth session view (below) is on. Its absence is not a problem: it is reported, not repaired.
- **Per-user**:
  - no leftover driver `RunOnce` entry exists (deleted if found, see Starting the driver);
  - the data and capture folder exists;
  - AI client integrations point at the current `SecondShell.Mcp.exe` (see AI client integration).

### Smooth session view (optional)

A child session is a Remote Desktop session, which Windows composes at about 30 fps; the session view then feels sluggish when the user takes control. `DWMFRAMEINTERVAL` = 15 (`HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server\WinStations`, DWORD) raises it to about 60 fps (65 fps measured, Findings.md 3), which is enough for most purposes; the host draws at the screen's own refresh rate either way.

- Offered as an option of the one-time setup (on by default), and in Settings; turning it on or off is a machine-wide step, so it needs elevation like the others.
- It applies to every Remote Desktop session on the machine from its next sign-in, including incoming connections, which then use more bandwidth and encoding. The setup dialog says so.
- Setup remembers whether the value existed before it set it; uninstall and turning it off remove it only if SecondShell set it.

### Repair

- **Per-user problems are fixed silently, without elevation.** They must be fixed by the unelevated process. If the user is a standard user, approving a UAC prompt runs the elevated process as a *different* (administrator) account, whose HKCU is a different registry hive.
- **Machine-wide problems** show one dialog explaining what is missing and why. Choosing "Repair" starts `SecondShell.exe --setup` as an elevated child process, which performs only the machine-wide steps and exits. The host waits for it and checks again.
- **If the user declines**, the window explains that the agent session can't start, and keeps a "Repair" command in its menu. MCP tools return a `SetupRequired` error with the same explanation.
- **Setup is idempotent**: it is safe to run any number of times, so first-time setup and repair are the same code.
- The window shows the setup state in a status line, so a broken setup is visible directly, not only as a failed connection.

### Uninstall

`SecondShell.exe --uninstall` removes any leftover driver `RunOnce` entry and the AI client integrations, removes the smooth session view setting if SecondShell set it (elevated), and offers to delete the saved password and the data folder. It leaves child sessions enabled, since other tools (e.g. the Windows Simulator) rely on them. The MSIX build offers the same cleanup in its settings, to run before the user uninstalls the package.

## Implementation

### Language and platforms

- **C++20** with **C++/WinRT**, built with MSVC, for **x64 and ARM64**. All executables are native; nothing needs a .NET runtime.
- **Host UI**: WinUI 3. The session view is a DirectX swap chain in the window (see Session display). The RDP control lives in a separate hidden Win32 window.
- **Driver, MCP server**: no UI framework.

### Libraries

[Axodox.Common](https://github.com/axodox/axodox-common) (NuGet package `Axodox.Common`, local repository `G:\dev\axodox-common`) is the base library for all components. It is shipped as a DLL next to the executables, and is built for x64 and ARM64 (its build script builds both, although its readme only mentions x64).

| Need | Axodox.Common module |
|---|---|
| Dependency injection, events (signals), logging, named enums, Win32/WinRT helpers | `Infrastructure` (`dependency_container`, `event_publisher` / `event_handler`, `logger`, ...) |
| Background threads, queues, cancellable async operations, dispatchers, locks | `Threading` (`background_thread`, `blocking_collection`, `async_operation`, ...) |
| Protocol messages between the components and MCP JSON-RPC messages | `Json` (`json_serializer`, `json_object_base` for declarative property binding) |
| **MCP tool input schemas and argument validation** | `Json` (`JsonSchema.h`): the JSON Schema of each tool is generated from the same C++ types that hold the parsed arguments, and incoming arguments are validated against it. One definition per action serves as parser, validator and the schema the AI client sees. |
| Streams, file IO, folder discovery | `Storage` |
| Direct3D 11 device, textures (including shared textures for captured frames), swap chains for the session view | `Graphics` (`GraphicsDevice`, `Texture2D`, `HwndSwapChain` / `CompositionSwapChain` / `XamlSwapChain`) |

Not covered by Axodox.Common, implemented in SecondShell (or contributed to Axodox.Common where generally useful):

- **Named pipe transport** between the components. Axodox.Common's messaging layer (`messaging_channel` / `messaging_server` / `messaging_client`) is transport-independent but only has TCP implementations. Named pipes are preferred over loopback TCP: they can be restricted to the current user, refuse remote clients, need no ports, and report the client's session ID for the session guard. A named-pipe implementation of the messaging layer is proposed as an addition to Axodox.Common (Architecture.md, section 9).
- **MCP protocol**: there is no official C++ MCP SDK. SecondShell implements the subset it needs (stdio transport, server side, tools only: `initialize`, `tools/list`, `tools/call`, `ping`, cancellation and progress notifications) on top of `Json`.
- **Windows features used directly**: the RDP ActiveX control (COM hosting), `Windows.Graphics.Capture`, `InputInjector`, WTS session APIs, Credential Manager and DPAPI, UI Automation, `Windows.Media.Ocr`.

Axodox.Common's `XamlSwapChain` is written for UWP XAML (`winrt::Windows::UI::Xaml::Controls::SwapChainPanel`), not WinUI 3 (`Microsoft::UI::Xaml`). The host needs a small WinUI 3 counterpart (via `ISwapChainPanelNative`), which could also be contributed to Axodox.Common.

### Coding conventions

SecondShell follows the coding conventions of Project Asgard (`H:\work\asgard`), documented in `H:\work\asgard-docs\Developer documentation\Coding-Conventions.md`. The main points:

- Two spaces for indentation, no tabs; opening braces on their own line (except initializer lists); always braces for loops.
- **Naming**: PascalCase for types, methods, public members and namespaces; camelCase for locals and arguments; `_underscoreCamelCase` for private members; `IMyInterface` for interfaces; `TItemType` for template arguments; `isSomething` / `hasSomething` for booleans. Code that provides basic library-like functionality (e.g. a named-pipe stream) uses snake_case like the standard library, WinRT and Axodox.Common.
- **Namespaces**: everything under the root namespace `SecondShell`; never `using namespace` in headers; alphabetically ordered `using namespace` lines in `.cpp` files.
- **Headers**: fixed member order (types, constants, public, protected, private); `.cpp` files follow the header's order.
- **Types and values**: `enum class` only; `auto` for locals but not for RAII objects; `type(expression)` casts for numbers; `std::unique_ptr` for owned dynamic objects, raw pointers to expose them; `override` together with `virtual`.
- **Errors**: exceptions with human-readable messages for rare failures; `winrt::check_hresult` and similar to turn error-code APIs into exceptions; error codes where failure is expected.
- **Warnings**: highest warning level with conformance mode; warnings don't fail the build, but each change should reduce them.
- **Files**: one type per file pair (`MyType.h` / `MyType.cpp`) with PascalCase file names, as in Axodox.Common.
- **Git**: snake_case branch names (`fix_` prefix for bug fixes), work branches from the latest `master`, pull requests for finished features.

## Deployment

SecondShell ships in two builds of the same binaries: a **portable** folder and an **MSIX** package. Portable comes first, being the simplest and enough for development and testing. MSIX follows once the design is stable.

### Portable

- A zip with a self-contained folder: the three executables and the shared library, with no installation and no runtime prerequisites. For .NET, trimming must be disabled (it breaks WinForms and COM interop; Native AOT is not possible for the same reason).
- Can run from any folder. The driver's `RunOnce` entry is written fresh for each new session with the current path, and the checks on every start keep the AI client integrations pointing at the current location, so moving the folder only needs a restart.
- Doesn't create a Start menu entry or any other shortcut; users can create one themselves.
- Should be code-signed: unsigned downloaded executables trigger SmartScreen, and an unsigned program that injects input is likely to be flagged by security software.

### MSIX

- Appears as a normal app in the Start menu.
- Desktop app with the `runFullTrust` capability (not a sandboxed AppContainer app), so named pipes, Credential Manager, DPAPI and input injection work as in the portable build.
- Registers **App Execution Aliases** with the same names as the executables: `SecondShell.exe` (host) and `SecondShell.Mcp.exe` (MCP server). Aliases stay the same across updates, unlike the versioned install folder. The AI client integrations use them.
- Starts the driver in the child session the same way as the portable build (`RunOnce`), if the registry redirection below allows it; otherwise through a **startup task** declared in the manifest. Packages are registered per user, so the packaged driver is available in the child session. *To verify.*
- Package identity (with the `inputInjectionBrokered` capability) may be required for gamepad injection with `InputInjector` (see Input injection).
- Updates through an `.appinstaller` file or the Microsoft Store.
- Must be signed with a trusted certificate (a CA or the Store). A self-signed certificate needs elevation to trust, which defeats the no-elevation goal.
- On first start (and on every start, see Setup and repair) it detects whether child sessions need enabling, and if so starts its elevated setup child process.

MSIX-specific behaviour to handle:

- **File redirection**: new files that a packaged app writes under `%LOCALAPPDATA%` are redirected into the package's own folder (`...\Packages\<package>\LocalCache\...`). Paths returned to the AI client (captures, logs) must be the real paths, or its file tools won't find them.
- **Registry redirection**: HKCU writes are redirected as well, so a `RunOnce` entry written from inside the package may be invisible to Explorer (see Starting the driver).
- **Elevated setup**: it is not yet verified that enabling child sessions works from inside the package context. If it doesn't, the setup step is run outside the package context.

### Build differences in code

The differences are kept behind one abstraction in `SecondShell.Common.dll`, selected at startup by whether the process has package identity (`GetCurrentPackageFullName`):

| | Portable | MSIX |
|---|---|---|
| Driver startup | `RunOnce` entry (HKCU) with the absolute path of `SecondShell.Driver.exe` | `RunOnce` entry if not redirected, otherwise startup task in the manifest (to verify) |
| Data and capture folder | `%LOCALAPPDATA%\SecondShell` | Real path of the package's redirected folder |
| MCP server path written into AI client configs | Absolute path of `SecondShell.Mcp.exe` | Full path of the `SecondShell.Mcp.exe` alias |
| Start menu entry | None (users can create a shortcut themselves) | Provided by the package |

## AI client integration

The SecondShell window has an **"Add to AI apps"** page. It lists the supported AI clients found on the machine, shows for each whether SecondShell is already configured, and adds, updates or removes the SecondShell entry on request. `SecondShell.exe --integrate <client>` and `--unintegrate <client>` do the same from the command line.

### Rules

- **Never overwrite the user's configuration.** Read the existing file, add or update only the `secondshell` entry, keep everything else (including formatting where possible), and write a backup (`<file>.bak`) first.
- **Use the client's own command-line tool where one exists** (e.g. `claude mcp add`, `code --add-mcp`). It knows its own file format and location better than SecondShell does.
- **Show what will change** before writing, and say whether the client needs a restart to pick it up.
- **Keep entries current**: the checks on every start detect entries that point at an old location (portable build moved) and offer to update them.
- **Uninstall removes all entries** that SecondShell added.

The entry runs the MCP server, with the path from Build differences in code and no arguments.

### Supported clients

Config locations and formats change between client versions, so each must be verified at implementation time. Each client is a small, separate adapter, so new clients are easy to add.

| Client | How | Config (current knowledge, to verify) |
|---|---|---|
| Claude Desktop | Edit config | `%APPDATA%\Claude\claude_desktop_config.json`, `mcpServers`. The Store/MSIX version of Claude Desktop may keep it in its package folder instead. |
| Claude Code | CLI | `claude mcp add --scope user secondshell -- <path>` |
| VS Code (GitHub Copilot) | CLI or edit config | `code --add-mcp "{...}"`, or the user-level `mcp.json` (`servers`, not `mcpServers`). Same for VS Code Insiders. |
| Visual Studio | Edit config | `%USERPROFILE%\.mcp.json`, `servers` |
| Cursor | Edit config | `%USERPROFILE%\.cursor\mcp.json`, `mcpServers` |
| Windsurf | Edit config | `%USERPROFILE%\.codeium\windsurf\mcp_config.json`, `mcpServers` |
| LM Studio | Edit config | `%USERPROFILE%\.lmstudio\mcp.json`, `mcpServers` |
| Gemini CLI | Edit config | `%USERPROFILE%\.gemini\settings.json`, `mcpServers` |
| OpenAI Codex CLI | Edit config | `%USERPROFILE%\.codex\config.toml`, `[mcp_servers.secondshell]` (TOML) |
| Bionic | To research | |
| Others (Cline, Continue, Zed, JetBrains AI Assistant, ...) | Later | Shown as "copy configuration" with a ready-to-paste snippet. |

For any client that isn't supported, the page offers a generic snippet to copy:

```json
{ "mcpServers": { "secondshell": { "command": "<path to SecondShell.Mcp.exe>" } } }
```
