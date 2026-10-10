# Stage 1 spikes

Throwaway programs for the risk spikes of [ImplementationPlan.md](../docs/ImplementationPlan.md) (stage 1). Results are in [Findings.md](../docs/Findings.md), sections 2.11 to 2.15. `Findings/` holds the older scripts behind sections 2.1 to 2.10.

| Program | Runs in | Spike | What it does |
|---|---|---|---|
| `RdpHost` | User's session | 1 (and host side of 2) | WinUI 3 app (unpackaged, self-contained). Hosts `MsRdpClient9NotSafeForScripting` with ATL in its own Win32 window that is never shown, signs in to a new child session with the `SecondShell:ChildSession` credential, arms a `RunOnce` entry for a program, writes the session guard file, waits for the program, logs the session off and cleans up. With `--frames` it also reads shared frames (spike 2). |
| `SessionCheck` | Child session | 1 | Every 2 s: repaints a marker window, captures the screen with GDI and checks the colour (fresh frames), moves the mouse with `InputInjector` and checks the cursor. |
| `SharedFrames` | Child session | 2 | `Windows.Graphics.Capture` of the session into a keyed-mutex shared texture, NT handles duplicated into the host process. |
| `Gamepad` | Child session (`--inject`), user's session (`--observe`, read only) | 3 | Injects a gamepad with `InputInjector` and reads Windows.Gaming.Input, XInput and GameInput. |
| `TouchPen` | Child session | 4 | Test window recording pointer and mouse messages; injects mouse buttons and wheels, touch and pen with `InputInjector`, then with `CreateSyntheticPointerDevice`. |
| `Breakaway` | User's session | — | Starts a program outside the MSIX package context of the caller (through Explorer), see Findings.md 2.12. |

Every program that injects input or captures the screen checks first that it runs in the child session the host expects (`exchange\expected_session.txt`, written after sign-in) and not in the console session, and exits otherwise.

## Running

Build each project (x64 or ARM64, Release), e.g. `MSBuild Spikes\RdpHost\RdpHost.vcxproj -p:Configuration=Release -p:Platform=x64`, then:

```powershell
.\Spikes\run-spike.ps1 -Label s1 -HostArgs '--thread','ui','--window','never' -Program SessionCheck -ProgramArgs '--seconds','60'
.\Spikes\run-spike.ps1 -Label s2 -HostArgs '--frames','--adapter','0' -Program SharedFrames -ProgramArgs '--seconds','30'
.\Spikes\run-spike.ps1 -Label s3 -Program Gamepad -ProgramArgs '--inject','--separate-reader'
.\Spikes\run-spike.ps1 -Label s4 -Program TouchPen
```

`run-spike.ps1` starts `RdpHost` through Explorer, waits for it, and copies the logs (`host.log`, `child.log`, ...) and the saved frame from `Spikes\exchange` to `Spikes\results\<Label>`.

`RdpHost` options: `--thread ui|dedicated` (thread owning the RDP control), `--window never|show-then-hide|offscreen|message`, `--winui shown|minimized|hidden`, `--class 7|9`, `--width`, `--height`, `--startup runonce|run`, `--frames`, `--adapter <DXGI index>`, `--frame-seconds`, `--timeout`.

`startup-probe.ps1` and `visibility-probe.ps1` are the diagnostics behind Findings.md 2.12; they must be run inside a package context to reproduce it.
