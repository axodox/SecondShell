# SecondShell – Investigation findings

What we learned from the Windows Simulator 16.0 (Visual Studio 2019 era) and from tests on this machine, to guide the SecondShell design: an MCP server that gives an AI agent its own Windows desktop to work in (e.g. testing desktop applications).

Status legend: **Verified** = tested on this machine (Windows 11 Pro 26300, 2026-10-10; sections 2.11 to 2.15: 2026-10-11). **From code** = read from decompiled Simulator sources. **Assumption** = not yet tested.

## 1. How the Windows Simulator works

The Simulator is a WPF shell around two Windows features. SecondShell needs none of its binaries.

| Piece | What it does | Source |
|---|---|---|
| Child sessions | A nested session of the *same user* on the same machine. Enabled once with `WTSEnableChildSessions(TRUE)` (admin; the Simulator does it through its elevated COM helper `UtilitiesElevated.dll`). Queried with `WTSIsChildSessionsEnabled` and `WTSGetChildSessionId`. | From code (`RDPSessionsHelper`, `App`) |
| RDP ActiveX (`mstscax.dll`, in-box) | Shows and drives the child session. `MSTSCLib.dll` / `AxMSTSCLib.dll` are only generated interop wrappers around it. | From code (`ChromeWindow.InitializeRDPHost`) |
| Code inside the session | A COM server is started inside the child session with the `Session:{id}!clsid:{...}` moniker. The Simulator uses this only for screenshots. | From code (`COMHelpers.CreateObjectInRemoteSession`, `ScreenshotHelper`) |
| Touch emulation | Native exports of `Utilities.dll`. Not needed for desktop testing. | From code (`RDPTouchHelper`) |

Key RDP client settings used by the Simulator:

- `IMsRdpExtendedSettings.set_Property("ConnectToChildSession", true)`
- `AdvancedSettings7.EnableCredSspSupport = true`
- `DesktopWidth` / `DesktopHeight`, plus `UpdateSessionDisplaySettings(...)` for resolution, scale and orientation changes while connected
- `DisableFullScreenHotkey = true`; drive, printer, port and device redirection off; clipboard redirection on
- On `discReason == 2055` (credentials rejected) it reconnects with `AllowPromptingForCredentials = true`

It also refuses to start if Credential Manager holds RDP credentials for `*`, `termsrv/*`, `localhost`, `termsrv/localhost` or `*session`, because those interfere with child-session sign-in.

## 2. Verified facts

### 2.1 Only one child session, and only one client for it

- With the Simulator connected to the child session, a second RDP client with `ConnectToChildSession` failed with `discReason=516` (connection refused) after ~16 s. No second session was created, and the Simulator's connection was unaffected.
- With the Simulator closed, the same client with the same settings got through to the sign-in step in 0.6 s.
- `WTSGetChildSessionId` returns a single ID, with no way to enumerate several.

**Consequence:** one agent desktop per machine. Parallel isolated desktops would need VMs (or Windows Server with RDS). SecondShell targets one desktop.

### 2.2 Single sign-on doesn't work here; a saved password does

- With single sign-on only (CredSSP, no password set), sign-in fails with `discReason=2055`. The Simulator works around this by showing a password prompt.
- With a generic Credential Manager entry `SecondShell:ChildSession` (user `AXODOX-PC\Axodox`) passed in through `UserName`, `Domain` and `AdvancedSettings7.ClearTextPassword`, sign-in succeeds with no prompt in about 2 s (`OnConnected`, then `OnLogonError -2`, then `OnLoginComplete`). `-2` appears to be informational, since sign-in completes right after it.
- The entry name is deliberately not `termsrv/*` / `localhost` (see section 1).

### 2.3 Disconnecting does not reliably end the session

*Corrected: an earlier version of this section said that disconnecting ends the session.*

- Closing the Windows Simulator ends the session within a few seconds (also seen in Task Manager). The Simulator logs off explicitly (`RDPSessionsHelper.LogOffChildSession`) when it exits.
- After one plain disconnect (the credential test, a few seconds after sign-in), the session also disappeared.
- But a session created at 21:01 stayed alive, disconnected, through five more connect/disconnect cycles over 45 minutes. Its apps (including a Notepad started by a test) kept running, and every new connection resumed it instead of signing in fresh. It ended only when logged off explicitly with `WTSLogoffSession`, which works without elevation.

**Consequence:** SecondShell must not rely on disconnecting to end the session. The host logs the child session off explicitly when it closes (as the Simulator does), and `reset_session` / `Shutdown-Session` do the same. A host crash may leave a disconnected session behind, and the next connection resumes it. The host should detect that and offer to resume or reset.

### 2.4 The user's startup apps also run in the child session

The child session is a full logon of the same user, so everything in the user's startup runs a second time inside it. On this machine: qBittorrent, Plex Media Server, PowerToys, iCloud, OpenVPN GUI and others. Duplicates of apps that use fixed network ports or exclusive data files can conflict with the user's own instances.

**Consequence:** the design must address this, e.g. by documenting it, detecting known conflicting apps, or offering to close startup apps in the child session after sign-in.

### 2.5 The RDP window can be hidden, but not minimized

A test program inside the child session captured the screen (GDI `CopyFromScreen`) and moved the mouse with `SendInput` every 2 seconds, while the host changed the state of its RDP window. Freshness was checked with a counter window the test program updated before each capture.

| RDP window | Capture in the session | Captured frames fresh | `SendInput` in the session |
|---|---|---|---|
| Visible | works | yes | works |
| Minimized | **fails** (`The handle is invalid`) | — | not determined |
| Hidden (`ShowWindow(SW_HIDE)`) | works | yes | works |
| Off-screen (-5000, -5000) | works | yes | works |

**Consequence:** the RDP control can live in a hidden window that is never minimized, while the host shows the session through frames captured by the driver. That avoids embedding the ActiveX control in the visible UI and lets the overlay be drawn in the same DirectX pipeline.

### 2.6 Typing into the session with `SendKeys`

- The host can type into the child session through `IMsRdpClientNonScriptable::SendKeys` (scan codes; the Win key is the extended code `0x15B`). Win+R, then a short App Paths alias, then Enter started a program in the child session. This worked in three runs.
- `SendKeys` fails with `E_FAIL` while the RDP window is minimized, and failed once while it was visible but probably not in the foreground. It seems to need the control to be active.
- Posting `WM_KEYDOWN` / `WM_KEYUP` to the control's internal input window (`IHWindowClass`) while the RDP window was hidden did **not** work: the messages were posted without error, but the program was not started.
- The RDP control's type library on this machine (118 types, up to `IMsRdpClientNonScriptable8`) has no other input interface.

**Consequence:** typing into the session from the host needs the RDP window to be active, which conflicts with the goal of working without focus.

### 2.7 COM activation with a per-user registration starts the server in the wrong session

A test COM server registered only under HKCU (no elevation) and activated with the `Session:9!clsid:{...}` moniker (the Simulator's method) was started in the **caller's** session (4), not session 9. The activation then failed after about 90 s with `0x80080005` (server execution failed). Cross-session activation probably needs a machine-wide registration with an AppID running as the interactive user (not verified).

**Consequences:** COM-based driver startup needs the elevated setup step. Any program meant for the child session must check its own session ID against the one the host expects and exit immediately if they differ. Without that check, this test program moved the user's real mouse and showed a window on the user's desktop for about a minute. With the check added, a repeat of the same COM activation started the test program in session 4 again, and it exited immediately (`GUARD: refusing to run in session 4 (expected file 10)`) without touching anything.

### 2.8 Starting a program in the child session as a startup app

Windows starts the user's startup apps at every logon, including the child session's. Both per-user registry keys were tested with the RDP window hidden from sign-in onwards, with no keystrokes and no focus:

| Entry | Started in the child session | Afterwards |
|---|---|---|
| `HKCU\...\CurrentVersion\Run` | yes, 8 s after sign-in | entry stays (runs at every logon) |
| `HKCU\...\CurrentVersion\RunOnce` | yes, 12 s after sign-in | entry consumed (deleted by Windows) |

In both cases the test program passed the session guard and then captured fresh frames and moved the mouse with `SendInput` in the child session. No elevation was needed.

**Consequence:** this is how SecondShell starts its driver: a `RunOnce` entry written by the host just before connecting to a new child session.

### 2.9 `InputInjector` works unpackaged in the child session

`Windows.UI.Input.Preview.Injection.InputInjector.TryCreate()` succeeds in an unpackaged process (Windows PowerShell 5.1), in the user's session and in the child session, even though the API is documented as needing the restricted capability `inputInjectionBrokered`. The injector exposes keyboard, mouse, touch, pen, gamepad and shortcut injection.

Test in the child session (started via `RunOnce`, RDP window hidden, after the session guard):

| Test | Result |
|---|---|
| Open a test file in Notepad | Notepad came to the foreground in the child session |
| `InjectKeyboardInput`, Unicode: `Hello world from InputInjector! ÁÉŐŰ` | Typed correctly, independent of the keyboard layout |
| `InjectKeyboardInput`, virtual keys: Ctrl+S | File saved; its content on disk matched the typed text exactly |
| `InjectKeyboardInput`, virtual keys: Ctrl+W | Tab closed |
| `InjectMouseInput`, absolute move to (300, 250) | Cursor at exactly (300, 250) |

Not yet tested: mouse buttons and wheels, touch, pen, gamepad (and whether XInput/GameInput games see an injected gamepad).

### 2.10 Apps in the child session use the user's own app state

In the 2.9 test, Notepad in the child session restored the user's own open Notepad tabs, including one with unsaved changes (`*qBittorrent-data.ini`), from the shared profile. It also wrote its window state (`LocalState\WindowState\*.bin`) back to the shared profile. The tab contents (`LocalState\TabState`) were not modified by the test.

The first stage 5 end-to-end run (2026-10-18) followed the plan's scenario: Win+R, `notepad`, type, capture. Notepad opened on the user's restored `qBittorrent-data.ini` tab, and the typed text went into it; the first word, typed 4 s after the launch, was lost while Notepad was starting. The test's clean-up (Ctrl+A, Delete, Ctrl+W) emptied the tab and brought up "Save changes?" with Save as the default button. Escape did not reach the dialog; clicking Cancel and undoing every change restored the tab, and nothing was saved.

**Consequence:** apps in the child session work on the user's real data and settings; see Design.md, Session lifecycle. Tests type only into apps that restore nothing from the profile, such as `conhost.exe cmd.exe /d`, and never clean up with keys that could save or close a document.

### 2.11 A WinUI 3 process can own the RDP control in a never-shown window (stage 1, spike 1)

`Spikes/RdpHost` is a WinUI 3 app built like the real host (unpackaged, self-contained, Windows App SDK 2.5.1). It hosts `MsRdpClient9NotSafeForScripting` with ATL (`AtlAxWin`) in its own top-level Win32 window, created without `WS_VISIBLE` and never shown. Its own WinUI window is shown without activation (`AppWindow.Show(false)`). In the child session, `Spikes/SessionCheck` (started by `RunOnce`, behind the session guard) checked every 2 s that a GDI capture shows a freshly painted colour and that an `InputInjector` mouse move lands exactly.

| RDP control on | Host window | Sign-in | Program started | Fresh captures | Mouse moves | Disconnects | Host took focus |
|---|---|---|---|---|---|---|---|
| WinUI UI thread (`MainSTA`) | never shown | 1.4 s | 12.8 s | 30 / 30 | 30 / 30 | 0 | no |
| Dedicated STA thread, WinUI window minimized | never shown | 1.4 s | 13.1 s | 15 / 15 | 15 / 15 | 0 | no |
| WinUI UI thread | message-only window (`HWND_MESSAGE` parent) | 1.3 s | 11.8 s | 10 / 10 | 10 / 10 | 0 | no |
| WinUI UI thread | off-screen (−5000, −5000), shown without activation | 1.4 s | 14.7 s | 10 / 10 | 10 / 10 | 0 | no |

- Events on connect: `OnConnecting`, `OnConnected` (0.3 s), `OnLogonError(-2)`, `OnLoginComplete` (1.3 to 1.4 s); on `WTSLogoffSession`: `OnDisconnected(3)`. `MsRdpClient7NotSafeForScripting` at 1024×768 behaves the same.
- The control reports `Connected` throughout, the window stays invisible and not iconic, and no child session is left after logoff.
- Event names were read from mstscax's type library at run time (`IMsTscAxEvents`), so the sink needs no hard-coded dispatch IDs.

**Consequence:** the host structure in Design.md works as planned. The control can live on the WinUI UI thread or on its own STA thread; both pump messages correctly. A message-only window is an option too: it can never appear in Alt+Tab, on the taskbar or on screen.

### 2.12 Processes started from a packaged app write into the package, not the real profile

The spikes were first run from a terminal inside the Claude desktop app, which is an MSIX package (`Claude_pzs8sxrjxfjjc`). Every process started from there inherited the package context, including `RdpHost`:

- Writes to HKCU (the `RunOnce` and `Run` entries, any other key) and below `%LOCALAPPDATA%` went into the package's private registry and `LocalCache`. The processes themselves saw their writes, but nothing outside the package did. Explorer in the child session enumerated `RunOnce` and `Run` and found none of the entries (Shell-Core events 9705/9706 with no 9707), and a program in the child session got `DirectoryNotFoundException` for the exchange folder.
- Writes to `AppData\Roaming` (the Startup folder), `C:\Users\Public` and other drives went through, and reads see the real state merged with the private one. This made the problem look like a Windows change at first.
- `PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY` (values 1, 2, 4, 5, 6) did not help: children started with it still wrote into the package.
- Asking the running Explorer to start the program (`IShellWindows` → desktop view → `IShellDispatch2::ShellExecute`, `Spikes/Breakaway --explorer`) did: that process writes to the real profile, and its `RunOnce` entry starts the program in the child session as in 2.8.

**Consequences:**

- AI clients can be packaged apps (the Claude desktop app is). `SecondShell.Mcp.exe` started by such a client runs in its package context, and a host started directly from it would too: its `RunOnce` entry would never reach Explorer. `HostLauncher` must start the host outside the caller's package context (through Explorer as above, or another broker), and the host should check at startup that it is not in one.
- This answers the MSIX open question in section 5 for virtualized processes: a `RunOnce` entry written with registry write virtualization on is redirected into the package. SecondShell's own MSIX build needs virtualization turned off for that key (or must write it from outside the package).

### 2.13 Shared frames across sessions work, on the same adapter (stage 1, spike 2)

`Spikes/SharedFrames` in the child session captures the primary monitor with `Windows.Graphics.Capture` and copies each frame into a texture created with `D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX`. It opens the host process (`OpenProcess(PROCESS_DUP_HANDLE)`, same user, other session), duplicates the texture handle and a small metadata section into it, and hands each frame over with the keyed mutex (key 0 → writer, key 1 → reader). `RdpHost --frames` opens the handle with `OpenSharedResource1` and reads frames for 15 to 20 s. A counter window repainting at ~60 Hz kept the screen changing.

| Host and writer adapter | Result | Rate | FrameArrived → host acquire | Release → acquire | Reader holds lock |
|---|---|---|---|---|---|
| NVIDIA RTX 5090 (the child session's display adapter) | frames arrive, content correct | 31.8 fps | p50 0.09 ms, p95 0.12 ms | p50 0.06 ms | p50 0.07 ms |
| AMD Radeon iGPU (both sides) | frames arrive | 31.8 fps | p50 0.14 ms, p95 0.19 ms | p50 0.12 ms | p50 0.10 ms |
| Writer NVIDIA, host AMD | `OpenSharedResource1` fails with `E_INVALIDARG` | — | — | — | — |

- `GraphicsCaptureSession.IsSupported()` is true in the child session, and `IsBorderRequired(false)` works unpackaged. The saved frame showed the child session's desktop with the counter window and cursor.
- In the child session, DXGI lists the NVIDIA adapter twice (a second LUID without outputs), the AMD iGPU and the Basic Render Driver; the session's output (`\\.\DISPLAY1`, 1920×1080) is on the NVIDIA adapter that the host also sees.
- Capture runs at ~32 fps although the screen changes at ~60 Hz: the child session composes at about 30 fps, presumably the RDP default (not verified; `DWMFRAMEINTERVAL` would be the knob, machine-wide).
- `Direct3D11CaptureFrame.SystemRelativeTime` is consistently ~30 ms (one frame) later than the moment the frame arrives, so it can't be used to measure capture latency; the table uses the writer's QPC time at `FrameArrived` (QPC is shared across sessions).
- Apart from a single ~100 ms outlier at startup, the transfer costs well under a millisecond. With one texture the writer skipped frames while the reader held it; the ring of textures planned in Architecture.md 3.6 avoids that.

**Consequence:** the session display design holds; no fallback to encoded frames is needed. Both processes must use the same adapter, as planned (the driver reports the LUID, the host creates its device on it).

### 2.14 Touch, pen, mouse buttons and wheels reach desktop apps (stage 1, spike 4)

`Spikes/TouchPen` in the child session records the pointer and mouse messages of a plain Win32 window (per-monitor DPI aware, 96 DPI) and injects input with `InputInjector`, then with the Win32 fallback (`CreateSyntheticPointerDevice` + `InjectSyntheticPointerInput`).

| Input | `InputInjector` | Synthetic pointer device |
|---|---|---|
| Mouse left, right, middle, X1 buttons; vertical and horizontal wheel | all received at the injected position | — |
| Touch tap, 20-step drag | `WM_POINTER*` type touch with pressure and orientation; promoted to `WM_LBUTTONDOWN/UP` | same |
| Two simultaneous touch contacts | two pointer IDs; the spread was also turned into a zoom gesture (`WM_MOUSEWHEEL`) | not tested |
| Pen hover, stroke with pressure, tilt, rotation | `WM_POINTER*` type pen, pressure 0.3 → 307/1024, tilt (15, −10), rotation 30; promoted to left button | same (no rotation set) |
| Pen barrel button | `PEN_FLAG_BARREL`, promoted to right button | not tested |
| Pen eraser | `Inverted` while hovering and `Inverted \| Eraser` in contact give `PEN_FLAG_INVERTED` / `PEN_FLAG_ERASER`; `Eraser` alone is ignored | not tested |

- `InjectedInputTouchInfo.Contact` is in absolute screen pixels (left, top, bottom, right), not offsets around the point; with offsets the contact area arrives empty.
- Pressure for `InputInjector` is 0..1 and arrives scaled to 0..1024.

**Consequence:** `InputInjector` covers mouse, touch and pen for desktop apps; the synthetic pointer fallback works too. Not tested: UWP / WinUI apps and multi-touch with the fallback.

### 2.15 An injected gamepad is invisible to Windows.Gaming.Input, XInput and GameInput (stage 1, spike 3)

`Spikes/Gamepad --inject` in the child session (foreground window) calls `InitializeGamepadInjection`, injects neutral, A + left stick X 0.5 + right trigger 1.0, B + left stick Y −0.75 and neutral (each re-sent every 100 ms for 2 s), then `UninitializeGamepadInjection`, while polling all three APIs every 50 ms.

- All calls succeed, but no API sees a new controller or any reading: `Gamepad.Gamepads` stays empty, `XInputGetState` reports no connected slot, GameInput (in-box v0 API, `GameInputCreate` S_OK) reports no gamepad reading or device. Only a real HID device of the machine (vid 0B05, pid 18E5) shows up as a `RawGameController`, in both sessions.
- The same with a separate reader process in the child session that owns the foreground window.
- A read-only observer in the user's session saw no new controller either.
- Not tested: whether a UWP / packaged app would see it, and injection in the user's own session (not allowed by the test rules).

**Consequence:** gamepad support cannot rely on `InputInjector` for desktop apps. The alternatives from Design.md remain: a virtual controller driver (elevation to install) or no gamepad support.

### 2.16 Desktop services in the child session (stage 6)

Found while building and testing the window, process, clipboard, wait and display actions (`SessionProbeTests`, 2026-10-18):

- **PowerShell errors**: with `-EncodedCommand`, `powershell.exe` writes errors to a redirected error stream as CLIXML (`#< CLIXML`), also with `-OutputFormat Text`. With `-Command` and the script as one argument, quoted by the C runtime's rules (double quotes in the script survive), they are text. Its error messages quote the script's line, so a prelude belongs on a line of its own.
- **Console windows** report the client process in them (e.g. `cmd.exe`) to `GetWindowThreadProcessId`, not the `conhost.exe` which owns them. An agent stops a console through that PID.
- **Hidden starts**: a console program started with `CREATE_NO_WINDOW` and an empty input stream has no window, and `cmd` exits at once; programs to look at or interact with have to start as Explorer would.
- **Ctrl+C to another console** from a GUI process (`AttachConsole`, `GenerateConsoleCtrlEvent`): the event reaches the sender too, on a thread the console creates afterwards. Handlers registered before attaching are not called, nor, apparently, after detaching; the default handler then ends the process (exit code 0xC000013A, no crash report). Registering a handler after attaching, and detaching only once it has seen the event, works (a test program, not kept; `SessionProbeTests` covers it). `SetConsoleCtrlHandler(NULL, TRUE)` would be inherited by child processes.
- **Foreground lock**: `SetForegroundWindow` from the driver is refused while another application has the foreground; input injected with `InputInjector` does not count as the driver's last input. A `SendInput` mouse move by nothing does, and changes nothing. `AttachThreadInput` to the foreground thread did not help.
- **Resolution changes** (`IMsRdpClient9::UpdateSessionDisplaySettings`): `GetSystemMetrics` reports the new size before captures do. The first frame after a change still has the old pool's texture size, with the new content size; growing, the next frame of the recreated pool has the new size. Captures and the size the host waits for are therefore taken from the newest frame.
- **OCR** (Windows.Media.Ocr, en-US) reads the console font's "ll" as "11" and misses a letter at the very edge of a window ("Microsoft" as "icrosoft" in a console's first column).
- **Clipboard**: every session has its own; with RDP clipboard redirection off, writes in the child session leave the user's clipboard alone. Windows adds the synthesized formats (`CF_TEXT`, `CF_OEMTEXT`, `CF_LOCALE` for text; `CF_BITMAP`, `CF_DIB` for `CF_DIBV5`). A console pastes what `Clipboard.Write` puts there.

- **The driver's start** waits for Explorer's startup programs, which it runs one at a time, in a fixed order: the machine's RunOnce and Run, the user's Run, packaged apps' startup tasks, then the user's RunOnce, where the driver's entry is. On this machine (2026-10-18, later the same day) that took 74 s: about 30 s of Explorer's own delay before startup programs, 30 s waiting for `SecurityHealthSystray.exe` (machine Run), which never became ready in the child session, 7 s for `iTunesHelper.exe`, and a few seconds for the rest. Earlier that day it took about 20 s. Explorer's logon tasks themselves finish within 2 s (Shell-Core events 62170/62171; 9705–9708 for the startup programs). `Initialize-Session` therefore waits 3 minutes by default.

**Consequences:** see Architecture.md 6.9.

## 3. Proposed architecture

```
Parent session (user's desktop)            Child session (agent's desktop)
┌───────────────────────────────┐          ┌──────────────────────────────┐
│ MCP server (stdio)            │  named   │ In-session agent             │
│  - checks/enables child       │  pipe    │  - screenshot                │
│    sessions                   │◄────────►│  - SendInput (mouse/keys)    │
│  - hosts mstscax, signs in    │          │  - UI Automation tree        │
│    from Credential Manager    │          │  - launch apps, list windows │
│  - keeps the connection alive │          │                              │
└───────────────────────────────┘          └──────────────────────────────┘
```

- **Host:** owns the RDP connection and the session lifecycle. Its window can be small and out of the way; the session resolution comes from `DesktopWidth/Height`, not from the window size.
- **Agent:** does all observation and input inside the session, so the user's real mouse and keyboard are never touched. UI Automation lets the model find and invoke controls by name, not only by pixels.
- **IPC:** a named pipe. The host accepts only clients whose `GetNamedPipeClientSessionId` matches the child session ID.
- **Starting the agent in the session** (simplest first): an HKCU Run entry that exits if no host pipe is waiting; a per-user COM server activated with the `Session:N!clsid:` moniker (as the Simulator does); or a SYSTEM service using `WTSQueryUserToken` + `CreateProcessAsUser`.
- **Sign-in flow:** read the `SecondShell:ChildSession` entry. If it's missing, ask once, through a foreground prompt or a `secondshell login` command. Save the entry only after `OnLoginComplete`. Delete it if it's rejected (`2055`).
- **Candidate MCP tools:**
  - `screenshot`
  - `click` / `type` / `key` / `scroll` / `drag`
  - `list_windows`, `get_ui_tree`, `find_element`, `invoke_element`
  - `launch_app`, `wait_for`
  - `reset_session`

## 4. Constraints

- Windows Pro or Enterprise (Home has no RDP host). Enabling child sessions needs admin once.
- The agent's desktop is the same user account, so files, registry and app settings are shared with the user's desktop. Input and display are isolated, data is not.
- Single-instance and packaged (Store) apps may hand off to the instance already running in the user's session.

## 5. Open questions

- ~~**Hidden host window**~~: answered in 2.5 (hidden or off-screen works, minimized doesn't).
- **Why do some disconnected child sessions log off and others persist?** See 2.3.
- ~~**Posted keyboard messages**~~: tested, don't work while hidden (2.6).
- ~~**Driver startup without focus**~~: answered in 2.8 (`RunOnce` entry, no elevation or focus needed).
- **MSIX**: does a `RunOnce` entry written from inside a package reach Explorer, or is it redirected into the package (see Design.md, Deployment)? *Partly answered in 2.12:* with registry write virtualization on, it is redirected. Still to test: SecondShell's own package with virtualization turned off for that key.
- ~~**`InputInjector`** for mouse buttons and wheels, touch, pen and gamepad~~: answered in 2.14 (mouse, touch, pen work) and 2.15 (gamepad is not seen by any API).
- **Starting the host from a packaged AI client** (2.12): which broker `HostLauncher` uses (Explorer's `ShellExecute` works), and how the host detects that it was started inside a package context.
- **Child session frame rate** (2.13, measured 2026-10-18): the child session composes at ~31.5 fps, whatever its windows do. A window repainting ~970 times a second gave 31.2 fps captured, and the host showed all of them; `GraphicsCaptureSession.MinUpdateInterval` = 1 ms (accepted) changed nothing. So the cap is the session's own compose rate, not capture or the host. The host side is done: the render thread waits for the host monitor's vertical blank (`DisplayClock`, 120 Hz measured on a 120 Hz screen) instead of an 8 ms poll, and input is forwarded at once. With `DWMFRAMEINTERVAL` = 15 (`HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server\WinStations`, DWORD, set by the user), a new child session composes at 65.2 fps, and the host shows every frame: child sessions honour it. 60 fps is enough, so lower values were not tried; it becomes an optional step of the one-time setup (Design.md, Smooth session view). Still open: drawing the user's own cursor locally.
- **ARM64**: stage 1 spikes build for ARM64 but were run only on x64.
- **First-time prompt:** a credential prompt from a background process (launched from VS Code) didn't come to the front. The first-time prompt needs a foreground-capable path.
- **Starting the agent in the session:** which of the three options to use. Check whether the `Session:N!clsid:` moniker works unelevated with a per-user (HKCU) COM registration.
- **Packaged app behaviour** in the child session, for the apps that will actually be tested.

- **Text recognition of console text** (2026-10-18): `Capture.WaitForText` failed on a console showing "All rights reserved". Over a busy photo wallpaper Windows OCR read almost nothing of the console ("windoids tversion"); with the console maximized it read the words, but out of order across lines ("...Corporation. All rights C: \Users .9711] reserved"), so a phrase is not found as one run. The capture itself was sharp, and the capture's `MinUpdateInterval` is not the cause (the same without it). ~~Next: rebuild lines from the words' positions, and upscale small text before recognition~~: measured in OcrOptions.md (window crops with rebuilt lines and fuzzy matching found every phrase), and built as the two-tier text waits (Design.md, Capture actions; Architecture.md 6.10), unit tested. Still open: the live check (`SessionProbeTests`, the waits for "All rights reserved"); whether the child session's capture reports dirty regions (the driver's log says "with" or "without dirty regions"; without, tile fingerprints are compared); UI Automation's cost on large trees (browsers, Explorer) in use; whether Windows OCR is safe to call from two waits at once (not serialized). If the quick tier misses too often in use, a PP-OCR model on ONNX Runtime as another `ITextRecognizer`.

## Test scripts

The scripts that produced these results are kept in [Spikes/Findings](../Spikes/Findings/README.md), with a table mapping each script to its section. The stage 1 spike programs (sections 2.11 to 2.15) are in [Spikes](../Spikes/README.md).

Versions for sections 2.11 to 2.15: Windows 11 Pro 26300.9550 (26H2); mstscax.dll 10.0.26100.9549; Windows App SDK 2.5.1 (Microsoft.UI.Xaml.dll 3.2.3.0); NVIDIA GeForce RTX 5090, driver 32.0.16.1714; AMD Radeon iGPU, driver 32.0.21042.62; GameInput.dll 0.2309.26100.9278; x64 only.
