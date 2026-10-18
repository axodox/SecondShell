# SecondShell – Implementation plan

The implementation of [Architecture.md](Architecture.md) in stages. Each stage ends with something that can be tested on its own, by unit tests, by the developer, or by hand. Later stages build on earlier ones but don't change them, except for fixes.

From stage 5 on, SecondShell can drive its own child session end to end. From then on, most behaviour can be tested by running sequences through the MCP server, without anyone watching the screen.

## Status (2026-10-11)

| Stage | State | Where |
|---|---|---|
| 0 Skeleton | Done | `master` (c6dd982) |
| 1 Spikes | Done; results in Findings.md 2.11–2.15; gamepad deferred | branch `stage_1_and_2` (ee19502) |
| 2 Transport, JSON-RPC | Done, tested (unit + real pipe) | branch `stage_1_and_2` (9f88267); Axodox.Common branch `named_pipe_messaging` (1b18ba7) |
| 3 Actions, protocol | Done, tested (52 SecondShell tests pass, x64 + ARM64 build) | branch `stage_1_and_2` (505c4a3) |
| 4 MCP server | Done, tested (68 SecondShell tests pass, incl. the MCP probe; x64 + ARM64 build); manual test in Claude Code open | branch `stage_1_and_2` (this commit, "Stage 4: MCP server"); Axodox.Common `named_pipe_messaging` (23b59e5) |
| 5 Session and driver core | Built, incl. credentials (uncommitted); unit tests pass (93, x64 + ARM64 build); first manual sign-in done: MCP → host launch → reset → prompt → sign-in → saved → driver accepted → Ready | branch `stage_1_and_2` ("Stage 5: session and driver core"); Axodox.Common `named_pipe_messaging` (ec9d54f) |

Notes for continuing:
- SecondShell builds against the local Axodox.Common (`Build\local.props`, gitignored, points to `G:\dev\axodox-common\Axodox.Common.Desktop`) until an Axodox.Common NuGet release contains the named pipe messaging.
- Commands run from the Claude desktop app's terminal are inside its MSIX package context: their HKCU and `%LOCALAPPDATA%` writes are redirected (Findings.md 2.12). Anything that must write the real profile (spike runs, later the host) is started through `Spikes\bin\...\Breakaway.exe --explorer`.
- Schema snapshots: `SecondShell.Tests\Snapshots\*.json`; rewrite them with `SECONDSHELL_UPDATE_SNAPSHOTS=1` after an intended change and review the diff. `Tools.json` is what AI clients see from `tools/list`.
- The MCP probe (`McpProbeTests`) runs `bin\<platform>\<configuration>\SecondShell.Mcp.exe` with the real pipe name, so it expects no SecondShell host to be running for the current user.
- Fixed during the first end-to-end run: `HostLauncher` asked the shell view's background object for `IShellFolderViewDual` directly (Explorer hands it out only as `IDispatch`; E_NOINTERFACE); `winrt::hresult_error` escaped `HostClient::Start` and `JsonRpcPeer::RunRequest` (abort); the sign-in loop used `reset_event::wait(0ms)`, which in Axodox waits forever (a zero timeout means none), so it hung after sign-in; it now tests the event with `operator bool`.
- Stage 5: the first sign-in is a manual test: `Initialize-Session` shows the Windows credential prompt over the SecondShell window; the password is saved (Credential Manager, `SecondShell:ChildSession`, DPAPI-encrypted) once the sign-in succeeds, so later runs, and the MCP probe, are unattended. The prompt's time counts toward the `Initialize-Session` timeout (60 s by default). Still open: verifying the rejected-password path (reasons 3847 and 4615 are from the documentation; 2055 was seen), and the "Forget saved password" command (settings page, stage 7) and its uninstaller step. Logs: `%LOCALAPPDATA%\SecondShell\Logs\host.log` and `driver.log`.
- Axodox.Common fixes made in stage 4: `named_pipe_messaging_client::connect` (one attempt, no retries), `supports_new` no longer breaks `dependency_container::resolve` of abstract types (MSVC 14.50), and polymorphic values parsed into a `value_ptr` keep their type when copied (they were sliced to the base).

## Testing approach

- **Unit tests** (`SecondShell.Tests`) for everything that doesn't need a session: JSON types, JSON-RPC, MCP handling, sequencing with a fake driver link, config editing.
- **MCP probe**: a small test client (`SecondShell.Tests` integration test or a console tool) that starts `SecondShell.Mcp.exe`, speaks MCP over stdio, runs scripted calls and checks the results. It allows end-to-end tests without an AI client.
- **Manual tests** where a person has to look or decide: the UI, visual overlay, interactive mode, AI client integrations, installation.
- **Safety in all tests**: anything that injects input runs only in the child session. The session guard is always active; there is no switch to disable it. Unit tests use fake injectors.

## Stage 0 – Solution skeleton

**Builds**: the solution with `SecondShell.Common`, `SecondShell` (WinUI 3, empty window), `SecondShell.Driver`, `SecondShell.Mcp`, `SecondShell.Tests`; Axodox.Common from NuGet; `pch.h` per project; warning level and conformance mode; x64 and ARM64 configurations; a build script.

**Test**: everything builds for x64 and ARM64; one trivial unit test runs; each executable starts and exits cleanly.

## Stage 1 – Risk spikes

Small throwaway programs (in a `Spikes` folder, not product code) for the assumptions that could still change the design:

| Spike | Question | Changes the design if it fails |
|---|---|---|
| ActiveX in a hidden window, from a WinUI 3 process | Can a WinUI 3 app host the RDP control in its own hidden Win32 window and connect to the child session? | Host process structure |
| Shared frames across sessions | `Windows.Graphics.Capture` in the child session, keyed-mutex shared texture, handle duplicated into a process in the user's session, same adapter: does the frame arrive, and how fast? | Session display (would fall back to sending encoded frames over the pipe) |
| Gamepad injection | Does an `InputInjector` gamepad in the child session reach `Windows.Gaming.Input`, XInput and GameInput? | Gamepad support (virtual controller driver, or none) |
| Touch and pen injection | Do `InputInjector` touch and pen reach a desktop app in the child session as pointer input? | Touch and pen implementation (Win32 synthetic pointer fallback) |

**Test**: run by the developer, using the child session setup from Findings.md (RunOnce, session guard). Results are recorded in Findings.md.

## Stage 2 – Transport and JSON-RPC

**Builds**:
- In Axodox.Common (`G:\dev\axodox-common`): named pipe messaging (`named_pipe_messaging_channel`, `_server`, `_client`, `named_pipe_security`) with its unit tests.
- In SecondShell.Common: `IMessageTransport`, `PipeTransport`, the JSON-RPC types and `JsonRpcPeer`, `CancellationToken`, `AppEnvironment`, `PipeNames`.
- In SecondShell.Tests: an in-memory transport pair for the tests.

**Test (unit)**:
- requests, responses, notifications, errors, cancellation, timeouts, over the in-memory transport and over a real pipe;
- the pipe refuses remote clients and other users;
- `PeerSessionId()` and `PeerProcessId()` are correct.

## Stage 3 – Action model and protocol

**Builds**: all action and result types (`SecondShell::Actions`), the protocol types (`SecondShell::Protocol`), `KeyNames`, `WindowId`, `ClipboardContent`.

**Test (unit)**:
- every action parses from its documented JSON (Design.md examples), with `"action"` as the discriminator;
- invalid input is rejected with a useful message;
- generated schemas match snapshots;
- results round-trip.

## Stage 4 – MCP server

**Builds**: `StdioTransport`, `McpServer`, `ToolSource`, `SessionToolSource` (all seven tools), MCP content types, `HostClient`, `HostLauncher`.

**Test**:
- **unit**: recorded MCP sessions (initialize, tools/list, tools/call, cancellation, errors);
- **MCP probe**: with no host running, `tools/list` returns the seven tools with their schemas, and calls return a clear error;
- **manual**: add `SecondShell.Mcp.exe` to Claude Code and check that the tools appear.

## Stage 5 – Session and driver core (first end-to-end)

**Builds**:
- **Host, without UI beyond a minimal window:**
  - `ChildSessionApi`, `CredentialStore`, `CredentialPrompt`, `SettingsStore`;
  - `RdpHostWindow`, `RdpClient`, `DriverLauncher`, `SessionManager`;
  - `DriverLink` with the session guard;
  - `McpRequestHandler` for `session/initialize`, `session/check`, `session/shutdown`;
  - a basic `SetupChecker`.
- **Driver:**
  - `DriverApplication` and `HostConnection` (guard handshake);
  - `ActionExecutor` with the mouse and keyboard handlers;
  - `InputInjectorProvider`, `MouseInjector`, `KeyboardInjector`, `InputStateTracker`, `MotionAnimator`;
  - `ScreenCapturer` and `CaptureEncoder` with the `Capture.Screen` / `Window` / `Region` actions.
- **Host sequencing:** `SequenceRunner` (with `Control.*`), `AgentControl`, `CaptureStore`, `ActionLog`, and `sequence/run`.

**Test**:
- **unit**: `SequenceRunner` with a fake driver link (order, async, Join, timeouts, failure handling, held-input reporting);
- **MCP probe, end to end**:
  - Initialize-Session starts the host, which signs in to the child session; the driver starts via RunOnce and is accepted;
  - a sequence opens Notepad with Win+R, types text, and captures the screen, and the capture shows the text;
  - Shutdown-Session logs the session off;
- **guard**: the driver started in the user's own session exits without doing anything;
- **manual (you)**: the first-time password prompt.

From here on, the developer can test on their own through the MCP probe.

## Stage 6 – Desktop services and waits

**Builds**:
- `WindowManager` and the window actions;
- `ProcessManager`, `ShellCommandBuilder` and the process actions;
- `ClipboardManager`, `ClipboardFormatConverter` and the clipboard actions;
- `TextRecognizer` and `ScreenConditionWaiter` (`WaitForText`, `WaitForStable`);
- the remaining error and timeout handling;
- `display/set` and `display/get` (`DisplayController`), and `capture/configure`.

**Test**:
- **unit**: `ShellCommandBuilder`, `ClipboardFormatConverter`, `KeyNames`, `CaptureStore` decisions;
- **MCP probe**: scripted scenarios covering every action. Examples: start a process and read its output; focus, move and close windows; clipboard round trip in each format; wait for text; change the resolution and check the capture size; timeouts and failures report correctly.

## Stage 7 – Host UI and session view

**Builds**:
- **Shared frames:** `SharedFrameWriter` / `SharedFrameReader`, `FramePublisher`, `FrameReceiver`, `WinUiSwapChain`, `SessionRenderer`.
- **Overlay:** `OverlayRenderer` with the cursor, keystroke, capture flash and activity elements, and `VisualizationFeed`.
- **Interactive mode:** read-only / interactive / Stop (`AgentControl` in the UI), `InteractiveInputForwarder`, `ClipboardSync`.
- **UI:** `MainWindow`, `SessionPage`, `ActionLogPage`, `StatusBar`, `ExistingSessionDialog`, `CloseSessionDialog`, and their view models and converters.

**Test**:
- **MCP probe**: the session keeps working while the window is minimized, resized or moved, and Stop interrupts a running sequence with `UserInterrupted`;
- **manual (you)**: the session view, overlay animations, interactive mode with mouse, keyboard and clipboard, and the behaviour of the dialogs.

## Stage 8 – Touch and pen

**Builds**: `TouchInjector`, `PenInjector` (`InputInjector`, with the synthetic pointer fallback; Findings.md 2.14), their actions and handlers, and the `TouchOverlay` element.

Gamepad support is deferred: an `InputInjector` gamepad is not seen by Windows.Gaming.Input, XInput or GameInput (Findings.md 2.15). Options for later: a packaged build with `inputInjectionBrokered`, or a virtual controller driver.

**Test**:
- **MCP probe**: a small test app in the child session (built for the purpose) records pointer input, and the probe compares it with the injected sequence;
- **manual (you)**: a real game or touch app, if wanted.

## Stage 9 – Setup, settings and AI client integrations

**Builds**:
- **Setup:** the full `SetupChecker`, `MachineSetup`, `ElevatedSetupLauncher`, `RepairDialog`, `Uninstaller`.
- **Settings:** `SettingsPage`.
- **Integrations:** `JsonConfigEditor`, `TomlConfigEditor`, `CliRunner`, the integrations, `IntegrationManager`, `IntegrationsPage`, `IntegrationPreviewDialog`, and `--integrate` / `--unintegrate`.

**Test**:
- **unit**: config editing (merge, backup, removal, preserving other entries), each integration against sample config files;
- **manual (you)**:
  - setup and repair on a machine with child sessions disabled (one UAC prompt);
  - adding SecondShell to the installed AI clients and using it from each;
  - uninstall.

## Stage 10 – Packaging

**Builds**:
- the portable zip for x64 and ARM64;
- code signing;
- the MSIX package (App Execution Aliases, the driver startup decision from the RunOnce redirection test, `AppEnvironment` paths) and an `.appinstaller` file.

**Test**:
- the portable build from a fresh folder on a clean machine or VM;
- the MSIX install, update and uninstall;
- the MCP probe suite on an ARM64 device or VM.

## Order and dependencies

```
0 Skeleton ─► 1 Spikes ─► 2 Transport ─► 3 Actions ─► 4 MCP ─► 5 Session core ─► 6 Desktop services ─► 7 UI ─► 8 Touch/pen/gamepad ─► 9 Setup & integrations ─► 10 Packaging
```

Stages 2 and 3 are independent of each other, and stage 4 needs only stages 2 and 3, so they can be done in any order. Stage 1's results may change stages 5, 7 and 8; Findings.md and the design documents are updated before those stages start.
