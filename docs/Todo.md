# Todo

Features to consider; not yet implemented.

## Capture Screen button

A **Capture Screen** button in the title-bar toolbar, between **Take control** and **Stop**. It takes a full-resolution PNG screen capture of the session display and puts it on the clipboard.

## Working while the screen is locked: what is left

Built: Keep working while the screen is locked, `InputUnavailable`, and `isUserPresent` / `input` in `Check-Session` (Design.md, Working while the screen is locked; Findings.md 2.17). Still open:

- **Driver start as a service**: a LocalSystem service that starts the driver in the child session as soon as it signs in (`WTSQueryUserToken`, `CreateProcessAsUser`), instead of the `RunOnce` entry that waits behind Explorer's startup programs (20 to 74 s, Findings.md 2.16), and exits in the user's own session. It does not help input while locked (Findings.md 2.17). Needs the elevated setup to install it. A separate change.
- **UI Automation actions** (invoke, toggle or set value on an element found by name): on hold. They do not cover canvas or 3D views, which need real mouse and touch input.
- **The window** could show that the session is being kept unlocked behind the lock screen, e.g. in the status bar, for when the user comes back.

## Output of asynchronous processes

An `async` process's console output isn't collected, so to find a line in a server's log the agent had to use Windows Terminal's search box through the UI. Add an option to write the output to a file, or a way to read it later. Being discussed: redirect it, and add actions to read it.

## Reset of a running session

`Initialize-Session` with `"existingSession": "Reset"` on a session that was already running returned the same session id and driver PID 3 times out of 5, without saying it hadn't reset anything. Either reset the running session (log it off and start a new one) or say that it didn't.

## Session lost right after starting

Twice, `Initialize-Session` returned `Ready` with the driver connected, and the next `Run-Sequence` failed with `SessionNotActive`; `Check-Session` then showed the driver disconnected. Once, the session ended on its own within 25 minutes of a `Set-Display` change from 3840×2160 back to 1920×1080 (possibly unrelated).

## Tighter window resize (optional)

Change the host window's resize logic so it leaves no empty space around the display view. This must not interfere with window swapping.

## Idle: show time since last active

While the agent is idle, show how long since it was last active (e.g. "3 h", "1 min", or similar).

## Log window performance with many entries

The log window is slow with ~2000 entries. Investigate and improve (e.g. UI virtualization / incremental rendering) so a large log stays responsive.

## Prompt for restart after enabling child sessions

After changing the registry settings that enable child sessions, the app should ask the user to restart — otherwise it fails.

## AppVeyor CI

Set up an AppVeyor pipeline that builds (x64 and ARM64), runs the tests, and produces the packaging artifacts (zips, MSIX bundle, `.appinstaller`).

## Microsoft Store shipping

Prepare for shipping through the Microsoft Store — Store-signed packaging, identity/manifest reserved in Partner Center, and whatever submission requirements apply.

## Tray icon shows agent activity

The tray icon should indicate when an agent is actively using the app.
