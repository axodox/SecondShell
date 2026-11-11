# Todo

Features to consider; not yet implemented.

## Working while the screen is locked: what is left

Built: Keep working while the screen is locked, `InputUnavailable`, and `isUserPresent` / `input` in `Check-Session` (Design.md, Working while the screen is locked; Findings.md 2.17). Still open:

- **UI Automation actions** (invoke, toggle or set value on an element found by name): on hold. They do not cover canvas or 3D views, which need real mouse and touch input.
- **The window** could show that the session is being kept unlocked behind the lock screen, e.g. in the status bar, for when the user comes back.

## Reset of a running session

`Initialize-Session` with `"existingSession": "Reset"` on a session that was already running returned the same session id and driver PID 3 times out of 5, without saying it hadn't reset anything. Either reset the running session (log it off and start a new one) or say that it didn't.

## Session lost right after starting

Twice, `Initialize-Session` returned `Ready` with the driver connected, and the next `Run-Sequence` failed with `SessionNotActive`; `Check-Session` then showed the driver disconnected. Once, the session ended on its own within 25 minutes of a `Set-Display` change from 3840×2160 back to 1920×1080 (possibly unrelated).

## Tighter window resize (optional)

Change the host window's resize logic so it leaves no empty space around the display view. This must not interfere with window swapping.

## AppVeyor CI

Set up an AppVeyor pipeline that builds (x64 and ARM64), runs the tests, and produces the packaging artifacts (zips, MSIX bundle, `.appinstaller`).

## Microsoft Store shipping

Prepare for shipping through the Microsoft Store — Store-signed packaging, identity/manifest reserved in Partner Center, and whatever submission requirements apply.

## Tray icon shows agent activity

The tray icon should indicate when an agent is actively using the app.
