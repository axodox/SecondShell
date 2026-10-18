# Handover (2026-10-18, late evening; continued in the desktop app after the VS Code extension crashed)

Where the work stands, for continuing after a break. The plan and its status table are in ImplementationPlan.md; this file covers what is not committed yet and what to know when picking it up.

## State

- **Branch** `stage_1_and_2`. Stages 0–6 and the Axodox.Common refactor are committed (Stage 6: `d876a85`).
- **Stage 7, host UI and session view: in progress; committed so far as "Stage 7 (part 1)".** The live session view, agent control and interactive mode work; 121 unit tests pass (x64).
- **Axodox.Common:** SecondShell uses the package `1.0.124-named-pipe-messaging` (in `packages`, built locally from `G:\dev\axodox-common`). Axodox has no uncommitted changes of mine.

## Stage 7 so far

Built and seen working: the child session's desktop drawn live in the SecondShell window, letterboxed, following window resizes. Resizing the window never resizes the session (the user wants it so, for now); only `Set-Display` does.

| Part | Files | What it does |
|---|---|---|
| Shared frame ring | `SecondShell.Common/SharedFrames.h/.cpp` | `SharedFrameWriter` (driver): 3 textures, NT handle + keyed mutex (key 0 both sides), writes into a texture the host does not hold. Shared section holds `Newest` = frame number << 8 \| index. `SharedFrameReader` (host): `TryLockNewer`, `LockNewest`. |
| Protocol | `DriverProtocol.h/.cpp` | `SharedFrames` (texture handles, state section, size, adapter LUID). `frames/subscribe` takes no host PID: the driver uses the pipe's peer, which the guard has checked. New notification `frames/changed`. |
| Driver | `FramePublisher.h/.cpp`; `ScreenCapturer` | `ScreenCapturer` raises `FrameArrived(context, texture, size)` under its lock, and has `Device()` and `AdapterLuid()`. `FramePublisher` answers `frames/subscribe`, writes frames only after a subscription, and makes a new ring + `frames/changed` on a size change. `Subscribe` reads the screen size before taking its own lock, so the lock order cannot deadlock. |
| Host | `DriverLink` (`SubscribeFrames`, `FramesChanged`), `FrameReceiver`, `WinUiSwapChain`, `SessionRenderer` | `FrameReceiver` subscribes when the session is Ready and owns the duplicated handles. `SessionRenderer`: GPU objects are made on the UI thread (`Rebuild`); the render thread polls every 8 ms, draws with Direct2D and presents; a mutex separates them. The device is created on the driver's adapter. `WinUiSwapChain` derives from Axodox's `SwapChain` (Axodox's `XamlSwapChain` is UWP-only); the render thread calls `ResizeIfChanged`, then redraws the newest frame. |
| Overlay | `VisualizationFeed`, `OverlayRenderer`, `OverlayElement`, `CursorOverlay`, `ActivityOverlay`, `CaptureFlashOverlay`, `ViewTransform` | Done and seen working (Architecture.md 6.10). `SequenceRunner` reports each step's start and finish; `SessionRenderer` draws the overlay after the frame and redraws while it animates. |
| UI | `MainWindow.xaml`, `App.xaml.cpp` | `SwapChainPanel x:Name="SessionView"` inside a Grid with the background (a `SwapChainPanel` must not get a `Background`: WinUI fails at startup with E_FAIL / 0x802B000A). A status line below. App attaches the renderer, and detaches it on close before the session shuts down. Host project defines `USE_DIRECTX`; pch includes `Axodox.Graphics.h` and `microsoft.ui.xaml.media.dxinterop.h`. |

### Since part 1 (`809c93d`), not committed

Clipboard sharing, the status bar, the log flyout, the dialogs, the startup view with the toolbar disabled until the session is ready, `Run-Sequence`'s `description`, the outlined icon. What each piece does: Architecture.md 6.10. Seen working live with scripted scenarios and window snapshots; 124 unit tests pass.

- `MainWindow::UseServices(container)`: not `Connect`, which the XAML compiler's `IComponentConnector` takes. The window, `StatusBar` and `ActionLogPanel` subscribe to their services themselves; `App` only builds the window and ties the session's end to it.
- UI Automation reaches the toolbar and dialog buttons by name ("Take control", "Stop", "Give back control", "Action log", "Reset", "Keep open"); the probe tests use it (`HostWindow`), and so can manual scripts.
- Test window for frame rates: `Spikes/exchange/animate.py` (tkinter, repaints as fast as it can; run in the session with `pythonw.exe`). `DWMFRAMEINTERVAL` = 15 is set on this machine.
- The icon is drawn by `Assets/make_icon.py`.

### Next for stage 7

1. **The user checks the visuals** by hand.
2. **Text recognition** misreads console text on busy screens (Findings.md 5); a separate task is proposed (rebuild lines from word positions, upscale), and probe tests waiting for text fail until then.
3. **From the mockups, not built yet**: the previous action dimmed beside the current one; a "refused" note when the agent sends sequences while the user has control; a running command's output live (needs streaming from the driver); Settings (stage 9).
4. **ImplementationPlan.md** status row 7, then the user commits.

## Open items, not blocking

- **PowerShell error text** quotes the UTF-8/progress prelude line along with the command. A fix is offered and not done: run the command as `& ([scriptblock]::Create('...'))`.
- **Driver start after sign-in** varies from about 20 s to 74 s; it waits for Explorer's startup programs (Findings.md 2.16). The `Initialize-Session` default timeout is now 3 minutes. The user decided to leave it, since the session normally stays running. Options if needed: `UserInitMprLogonScript`, or a logon-triggered scheduled task.
- **Rejected-password path** not verified (reasons 3847/4615); "Forget saved password" belongs to stage 7 settings.
- **Pre-existing warnings**: C4459 `Error` in Ws2tcpip.h (the driver's pch), C4310 / C4458 from Axodox headers.

## Working notes

- **Tests**:
  - Unit tests: `vstest.console.exe bin\x64\Debug\SecondShell.Tests.dll /Platform:x64 /TestCaseFilter:"FullyQualifiedName!~SessionProbeTests"`.
  - Session tests: the same with `FullyQualifiedName~SessionProbeTests`. They sign in once per class and log off at the end; they need the saved password and no running host.
  - The native test adapter ignores `TestCategory`, hence the filter by class name.
- **Snapshots**: rewrite with `SECONDSHELL_UPDATE_SNAPSHOTS=1`, then review the diff of `SecondShell.Tests/Snapshots/*.json`.
- **Before rebuilding**: log the session off and close the host window (it locks `SecondShell.Common.dll` and `Axodox.Common.dll`).
- **A stalled build** shows idle `cl.exe` and `mspdbsrv` processes with almost no CPU. End them, delete the project's `obj\x64\Debug\<project>` folder, and build again. It happened twice after a `pch.h` change.
- **Manual probing**: a small Python MCP client and scenario scripts are in this session's scratchpad (`probe.py`, `s_*.py`; `snap_host.py` saves the host window with PrintWindow and `PW_RENDERFULLCONTENT`). The scratchpad is temporary, so recreate them if gone.
  - Messages to `SecondShell.Mcp.exe` must be single-line JSON.
  - Run Python with `PYTHONIOENCODING=utf-8`, and open files with `encoding="utf-8"` (the Windows default is cp1252).
- **Test applications**: consoles only (`conhost.exe cmd.exe /d`), never Notepad. The child session shares the user's profile, and Notepad opens the user's own tabs (Findings.md 2.10).
- **Shell traps**:
  - Backslashes in Bash heredocs get mangled; write files containing backslashes with the Write or Edit tool.
  - The shell does not search the current directory for programs: call `.\program.exe`.
  - Axodox's `wait(0ms)` waits forever.
