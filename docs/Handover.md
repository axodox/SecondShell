# Handover (2026-10-18, night)

Where the work stands, for continuing after a break. The plan and its status table are in ImplementationPlan.md; this file covers what to know when picking it up.

## State

- **Branch** `stage_1_and_2`. Stages 0–8 and the Axodox.Common refactor are committed; nothing is pushed.
- **Stage 10, packaging: in progress.** The portable part is done: `package.ps1` makes the signed zips for x64 and ARM64; the x64 zip ran end to end from a fresh folder. Next: the MSIX package (App Execution Aliases, so AI apps run a path that survives updates; the RunOnce check; `.appinstaller` on GitHub releases).
- **Stage 9, setup, settings and AI client integrations: done.** The user's hands-on check is to come (below).
- **Stage 8, touch and pen: done.** x64 and ARM64 build.
- **Stage 7, host UI and session view: done.** The user's hands-on check is still to come (below).
- **Axodox.Common:** SecondShell uses the package `1.0.124-named-pipe-messaging` (in `packages`, built locally from `G:\dev\axodox-common`). Axodox has no uncommitted changes of mine.

## Stage 9, part 1: setup and repair

`SetupChecker` at every start; `SecondShell.exe --setup [--smooth-view=on|off]` run elevated by `ElevatedSetupLauncher`; the one-time setup dialog, the setup notice with Set up in the session view, "Setup needed" in the status bar. Architecture.md 6.12.

- **Manual test (the user)**: on a PC without child sessions (or after `WTSEnableChildSessions(FALSE)`), the dialog at start, Set up with the administrator prompt, declining it, and the smooth view option. Here child sessions are on and `DWMFRAMEINTERVAL` = 15 was set by hand, so SecondShell does not count it as its own.
- **Checked here**: the dialog and the notice (with a local switch faking the missing setup, not committed); `SecondShell.exe --setup` unelevated with nothing to do exits 0 without a window.
- `MachineConfiguration` is the only code that writes the machine's settings; tests use a fake (`SetupTests`).

## Stage 10, part 1: portable

`package.ps1 [-Platforms x64,ARM64] [-SkipBuild] [-CertificateSubject "CN=Axodox"] [-TimestampUrl <url>]`: builds Release, stages `artifacts\<platform>\SecondShell` (the Release output without symbols, build leftovers and the test projects, plus the Visual C++ runtime from Visual Studio's redist folder), signs SecondShell's own five binaries with the newest valid certificate of that subject in `Cert:\CurrentUser\My`, and writes `artifacts\SecondShell-<version>-<platform>.zip` (x64: 24 MB, 259 files) and `...-symbols.zip`.

- **Certificate**: self-signed, `CN=Axodox`, code signing, RSA 3072, valid until 2036-10-18, thumbprint `89D3A530B13C9FA9FF38A4A5C39FAFEE786F69FF`, its private key exportable, only in this user's store (created through `Breakaway --explorer`, as this terminal's store writes would be redirected into the Claude app's package). Back it up (export with its key) to sign on another machine; Windows does not trust it, so the signatures show `UnknownError` and SmartScreen still warns.
- **Version**: `SecondShellVersion` in `Build\SecondShell.Cpp.props` feeds `ProductInfo::Version`, the version resource of the executables (`Assets\SecondShell.rc`) and the zip names.
- **Tested here**: the x64 zip unzipped to `obj\portable-test`: the MCP server there started the host there, which started the driver there in the child session; a console, typing, a text wait and a capture worked, and the session shut down. Not tested: a clean machine without the Visual C++ runtime installed (Windows Sandbox would do), and the ARM64 zip on an ARM64 device.
- **Release-only bug found and fixed**: DriverLink and McpRequestHandler destroyed a disconnected connection inside the channel's disconnected event, whose next handler was the destroyed transport's (6ca49da).
- **Build "stalls" were the output pipe**: piping a build into `Select-Object -First N` (or `head`) stops reading its output; with Release's many warnings the pipe fills, and MSBuild and the compilers block writing to it (seen as idle `cl.exe`). Write the whole output to a file and filter that. `build.ps1` now also asks for the 64-bit compilers (`PreferredToolArchitecture=x64`), which ARM64 builds otherwise do not use.

## Stage 9, part 4: uninstall

`SecondShell.exe --uninstall [--forget-password] [--delete-data]`, and Settings, General, "Remove SecondShell from this PC" (both choices offered). Architecture.md 6.12.

- **Checked here**: the dialog (cancelled); `UninstallerTests` with a test credential and temporary folders.
- **Not tried**: a real uninstall (it would take SecondShell out of the AI apps here, and the smooth view step asks for administrator rights).

### Stage 9: the user's hands-on check

1. One-time setup on a PC without child sessions (or with them turned off): the dialog, the administrator prompt, declining it.
2. Settings: a change of each kind, then a new session (display, sign-in timeout) and a sequence (shell, captures); Forget saved password; the smooth view switch.
3. Add to AI apps: add, update and remove for the apps used here, and check each app sees SecondShell; `--integrate` / `--unintegrate` from PowerShell (`Start-Process -Wait`).
4. Uninstall, last.

## Stage 9, part 3: Add to AI apps

Settings, Add to AI apps: ten apps with their state, Add / Update / Remove behind a preview of the change; `--integrate` / `--unintegrate`. Client locations checked against current docs (Design.md, Supported clients) and Asgard's `Holomaps.Mcp\McpInstaller.cpp`, from which the Claude Desktop MSIX path, the LM Studio folders, finding an entry by its command and updating only its command were taken. Architecture.md 6.6, 6.12.

- **Checked here**: the page and the Cursor preview (cancelled), `--integrate` with an unknown app (exit 2); `IntegrationTests` against temporary folders (no real config, no `claude` run).
- **Not tried**: adding to a real app (the user's choice which; from this terminal the writes to `%APPDATA%` would land in the Claude app's package, Findings.md 2.12), and the Claude Code command line for real.

## Stage 9, part 2: settings

`SettingsStore` (`settings.json` in the data folder) feeds the defaults of `Initialize-Session`, captures and `Process.Run`, and the capture retention. The Settings button opens `SettingsView` (General, About) in place of the session view; Back or Escape returns. Architecture.md 6.12.

- **Checked here**: the pages' look (screenshots, without changing a setting), Back, and the unit tests for the store and the default shell.
- **Not tried**: changing settings from the page (each change writes `settings.json`; from this terminal the write would land in the Claude app's package, Findings.md 2.12), Forget saved password, and the smooth view switch (needs the administrator prompt). The user's hands-on check.

## Stage 8

Touch and pen for the agent, and for the user in interactive mode. What each piece does: Architecture.md 6.11; the pen actions: Design.md, Pen input.

| Part | Files | What it does |
|---|---|---|
| Actions | `SecondShell.Common/TouchActions.h`, `PenActions.h`, `Actions.cpp`; `InputState::Pen` | `Touch.*` (existed) and the new `Pen.Move` / `Press` / `Release` / `Tap` / `Reset`; `releasedInput.pen` |
| Driver | `TouchActionHandler`, `PenActionHandler`, `InputStateTracker`, `TouchInjector`, `PenInjector`, `InjectionHeartbeat`, `ScreenPosition` | Handlers check and animate; the tracker holds fingers and the pen, so resets lift them; the injectors repeat the last frame every 50 ms (Windows drops it after ~100 ms) |
| Host | `TouchOverlay`, `PenOverlay`, `OverlayElement::Motion`; `InteractiveInputForwarder::OnTouch` / `OnPen` / `OnPointerGone`, `MainWindow::ForwardPointer` | Overlay discs, ripples, pen ring, dot and trail; the user's touch and pen forwarded as such |
| Tests | `DriverInputTests`; `SessionProbeTests` (3 new); `SecondShell.PointerRecorder` | Fakes for frames and pen states; live: taps, a held finger, two fingers, pen pressure, tilt, rotation, barrel, eraser, a failed sequence lifting both |

### Notes

- **Turning the pen over**: Windows ignores a change between tip and eraser while the pen is in range, so `PenInjector` takes it out of range and back (a new pointer ID each time, as with a real pen).
- **No Win32 fallback**: the plan's synthetic pointer injectors are not built; mouse and keyboard have no fallback either (Architecture.md 4.2).
- **PointerRecorder** records to the file on its command line; the tests put it in `bin\x64\Debug\PointerRecords` (not under AppData: this terminal's writes there are virtualized) and delete it afterwards.

### Still to do

- **The user's hands-on check**, if a touch screen or pen is at hand: the overlay for the agent's touch and pen, and the own touch and pen in interactive mode (only built, not tried: this machine has neither).

## Stage 7

The child session's desktop drawn live in the SecondShell window, letterboxed, following window resizes. Resizing the window never resizes the session (the user wants it so, for now); only `Set-Display` does. What each piece does: Architecture.md 6.10.

| Part | Files | What it does |
|---|---|---|
| Shared frame ring | `SecondShell.Common/SharedFrames.h/.cpp` | `SharedFrameWriter` (driver): 3 textures, NT handle + keyed mutex (key 0 both sides), writes into a texture the host does not hold. Shared section holds `Newest` = frame number << 8 \| index. `SharedFrameReader` (host): `TryLockNewer`, `LockNewest`. |
| Protocol | `DriverProtocol.h/.cpp` | `SharedFrames` (texture handles, state section, size, adapter LUID). `frames/subscribe` takes no host PID: the driver uses the pipe's peer, which the guard has checked. New notification `frames/changed`. |
| Driver | `FramePublisher.h/.cpp`; `ScreenCapturer` | `ScreenCapturer` raises `FrameArrived(context, texture, size)` under its lock, and has `Device()` and `AdapterLuid()`. `FramePublisher` answers `frames/subscribe`, writes frames only after a subscription, and makes a new ring + `frames/changed` on a size change. `Subscribe` reads the screen size before taking its own lock, so the lock order cannot deadlock. |
| Host | `DriverLink` (`SubscribeFrames`, `FramesChanged`), `FrameReceiver`, `WinUiSwapChain`, `SessionRenderer` | `FrameReceiver` subscribes when the session is Ready and owns the duplicated handles. `SessionRenderer`: GPU objects are made on the UI thread (`Rebuild`); the render thread waits for the monitor's vertical blank (`DisplayClock`), draws with Direct2D and presents; a mutex separates them. The device is created on the driver's adapter. `WinUiSwapChain` derives from Axodox's `SwapChain` (Axodox's `XamlSwapChain` is UWP-only); the render thread calls `ResizeIfChanged`, then redraws the newest frame. |
| Overlay | `VisualizationFeed`, `OverlayRenderer`, `OverlayElement`, `CursorOverlay`, `CaptureFlashOverlay` (the activity list is now `ActivityCards`, in WinUI), `ViewTransform` | Done and seen working (Architecture.md 6.10). `SequenceRunner` reports each step's start and finish; `SessionRenderer` draws the overlay after the frame and redraws while it animates. |
| UI | `MainWindow.xaml`, `Theme.xaml`, `StatusBar`, `ActionLogPanel`, `ActionView`, `SequenceView`, `ActivityCards`, `SessionDialogs` | `SwapChainPanel x:Name="SessionView"` inside a Grid with the background (a `SwapChainPanel` must not get a `Background`: WinUI fails at startup with E_FAIL / 0x802B000A). App attaches the renderer, and detaches it on close before the session shuts down. Host project defines `USE_DIRECTX`; pch includes `Axodox.Graphics.h` and `microsoft.ui.xaml.media.dxinterop.h`. |
| Agent control | `AgentControl`, `InteractiveInputForwarder`, driver `UserInputHandler`, `ClipboardSync` / `ClipboardMirror` / `ClipboardSyncHandler` | Stop, Take control, Give back; the user's input and clipboard while they have control. |
| Text waits | driver `TextSearch` and the classes around it | Two tiers on dirty regions, packed atlases, swappable recognizers (OcrOptions.md, Decided design). Merged from `claude/eager-hodgkin-dc9e33`. |

### Notes

- `MainWindow::UseServices(container)`: not `Connect`, which the XAML compiler's `IComponentConnector` takes. The window, `StatusBar` and `ActionLogPanel` subscribe to their services themselves; `App` only builds the window and ties the session's end to it.
- UI Automation reaches the toolbar and dialog buttons by name ("Take control", "Stop", "Give back control", "Action log", "Reset", "Keep open"); the probe tests use it (`HostWindow`), and so can manual scripts.
- Test window for frame rates: `Spikes/exchange/animate.py` (tkinter, repaints as fast as it can; run in the session with `pythonw.exe`). `DWMFRAMEINTERVAL` = 15 is set on this machine.
- The icon is drawn by `Assets/make_icon.py`.

- Text waits see text anywhere on the screen, at once from the cache: a test or agent waiting for "a new window is ready" must not use text another window may show too (e.g. a console banner); the probe tests wait for the new window (`WaitForNewWindow`).

### Still to do

1. **The user's hands-on check** (the plan's manual test), in particular what only scripts have checked so far: clipboard sharing in both directions, closing while the agent works, the dialog for a session left by a crashed host.
2. **Deferred to a later polish pass**, from the mockups: the previous action dimmed beside the current one; a "refused" note when the agent sends sequences while the user has control; a running command's output live (needs streaming from the driver). Settings come with stage 9.

## Open items, not blocking

- **PowerShell error text** quotes the UTF-8/progress prelude line along with the command. A fix is offered and not done: run the command as `& ([scriptblock]::Create('...'))`.
- **Driver start after sign-in** varies from about 20 s to 74 s; it waits for Explorer's startup programs (Findings.md 2.16). The `Initialize-Session` default timeout is now 3 minutes. The user decided to leave it, since the session normally stays running. Options if needed: `UserInitMprLogonScript`, or a logon-triggered scheduled task.
- **Rejected-password path** not verified (reasons 3847/4615); "Forget saved password" belongs to the settings (stage 9).
- **Pre-existing warnings**: C4459 `Error` in Ws2tcpip.h (the driver's pch), C4310 / C4458 from Axodox headers.

## Working notes

- **Tests**:
  - Unit tests: `vstest.console.exe bin\x64\Debug\SecondShell.Tests.dll /Platform:x64 /TestCaseFilter:"FullyQualifiedName!~SessionProbeTests"`.
  - Session tests: the same with `FullyQualifiedName~SessionProbeTests`. They sign in once per class and log off at the end; they need the saved password and no running host.
  - The native test adapter ignores `TestCategory`, hence the filter by class name.
- **Snapshots**: rewrite with `SECONDSHELL_UPDATE_SNAPSHOTS=1`, then review the diff of `SecondShell.Tests/Snapshots/*.json`.
- **Before rebuilding**: log the session off and close the host window (it locks `SecondShell.Common.dll` and `Axodox.Common.dll`).
- **A stalled build** shows idle `cl.exe` and `mspdbsrv` processes with almost no CPU: something stopped reading the build's output (e.g. `Select-Object -First N`), and the compilers block writing their warnings. Never cut a build's output short; write it to a file. End the processes, delete the project's `obj\<platform>\<configuration>\<project>` folder, and build again.
- **Manual probing**: a small Python MCP client and scenario scripts are in this session's scratchpad (`probe.py`, `s_*.py`; `snap_host.py` saves the host window with PrintWindow and `PW_RENDERFULLCONTENT`). The scratchpad is temporary, so recreate them if gone.
  - Messages to `SecondShell.Mcp.exe` must be single-line JSON.
  - Run Python with `PYTHONIOENCODING=utf-8`, and open files with `encoding="utf-8"` (the Windows default is cp1252).
- **Test applications**: consoles only (`conhost.exe cmd.exe /d`), never Notepad. The child session shares the user's profile, and Notepad opens the user's own tabs (Findings.md 2.10).
- **Shell traps**:
  - Backslashes in Bash heredocs get mangled; write files containing backslashes with the Write or Edit tool.
  - The shell does not search the current directory for programs: call `.\program.exe`.
  - Axodox's `wait(0ms)` waits forever.
