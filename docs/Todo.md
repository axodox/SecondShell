# Todo

Features to consider; not yet implemented.

## Capture Screen button

A **Capture Screen** button in the title-bar toolbar, between **Take control** and **Stop**. It takes a full-resolution PNG screen capture of the session display and puts it on the clipboard.

## Working while the screen is locked

Investigate whether capture/control can be made to work while the machine's screen is locked for a long time — it currently returns access denied. Find out what access the capture and input paths lose at the lock screen and whether there is a supported way around it.

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
