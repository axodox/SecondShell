# SecondShell – Investigation findings

What we learned from the Windows Simulator 16.0 (Visual Studio 2019 era) and from tests on this machine, to guide the SecondShell design: an MCP server that gives an AI agent its own Windows desktop to work in (e.g. testing desktop applications).

Status legend: **Verified** = tested on this machine (Windows 11 Pro 26300, 2026-10-10). **From code** = read from decompiled Simulator sources. **Assumption** = not yet tested.

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

**Consequence:** apps in the child session work on the user's real data and settings; see Design.md, Session lifecycle.

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
- **MSIX**: does a `RunOnce` entry written from inside a package reach Explorer, or is it redirected into the package (see Design.md, Deployment)?
- **`InputInjector`** for mouse buttons and wheels, touch, pen and gamepad in the child session, and whether XInput/GameInput games see an injected gamepad (2.9).
- **First-time prompt:** a credential prompt from a background process (launched from VS Code) didn't come to the front. The first-time prompt needs a foreground-capable path.
- **Starting the agent in the session:** which of the three options to use. Check whether the `Session:N!clsid:` moniker works unelevated with a per-user (HKCU) COM registration.
- **Packaged app behaviour** in the child session, for the apps that will actually be tested.

## Test scripts

The scripts that produced these results are kept in [Spikes/Findings](../Spikes/Findings/README.md), with a table mapping each script to its section.
