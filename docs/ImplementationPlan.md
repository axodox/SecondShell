# SecondShell – Implementation plan

The implementation of [Architecture.md](Architecture.md) in stages. Each stage ends with something that can be tested on its own, by unit tests, by the developer, or by hand. Later stages build on earlier ones but don't change them, except for fixes.

From stage 5 on, SecondShell can drive its own child session end to end. From then on, most behaviour can be tested by running sequences through the MCP server, without anyone watching the screen.

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
- In SecondShell.Common: `IMessageTransport`, `PipeTransport`, an in-memory transport for tests, the JSON-RPC types and `JsonRpcPeer`, `CancellationToken`, `AppEnvironment`, `PipeNames`.

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

## Stage 8 – Touch, pen, gamepad

**Builds**: `TouchInjector`, `PenInjector`, `GamepadInjector` (or the alternatives chosen in stage 1), their actions and handlers, and the `TouchOverlay` and `GamepadOverlay` elements.

**Test**:
- **MCP probe**: a small test app in the child session (built for the purpose) records pointer and gamepad input, and the probe compares it with the injected sequence;
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
