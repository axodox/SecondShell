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
