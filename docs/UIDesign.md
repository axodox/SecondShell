# SecondShell UI design

The look and behaviour of the host window (`SecondShell.exe`), agreed on 2026-10-18 from mockups. Design.md says what the UI must do; this file says how it looks.

## Mockups

Static HTML copies in [`ui/`](ui/), each opening in a browser:

| File | Shows |
|---|---|
| [Main.html](ui/Main.html) | The session while the agent works: title bar and toolbar, the session with its frame and the overlay, the status bar |
| [StatusBar.html](ui/StatusBar.html) | The status bar in its states: just updated, a long command, keys, between sequences, a failed step, the user in control |
| [LogFlyout.html](ui/LogFlyout.html) | The action log flyout, with a capture's image and a command's live output |
| [Interactive.html](ui/Interactive.html) | The user in control |
| [Settings.html](ui/Settings.html) | Settings, General |
| [AiApps.html](ui/AiApps.html) | Settings, Add to AI apps |
| [Dialogs.html](ui/Dialogs.html) | Existing session, closing while the agent works, integration preview, one-time setup |

The editable originals are on the design canvas https://claude.ai/artifact/AvvMY5BswkGbsuVnfYFAQK (private to its owner). The copies here are made from it and are the reference if the two differ.

## Implementation approach

- **Built-in WinUI 3 controls**, so behaviour, accessibility and keyboard handling come with them.
- **Custom styles** for them, and template overrides where a style cannot reach (e.g. the toolbar buttons, the log flyout, settings rows).
- **One theme**: colours, corner radii, spacing and fonts are resources in theme dictionaries, which the styles reference, so the look can change in one place.
- **Custom controls only where WinUI has none**: the argument and keycap rectangles, the activity bars. The session frame and the overlay are drawn by `SessionRenderer`.
- The aim is the mockups' theme and overall look, not a pixel copy.

## Theme

| Token | Value | Use |
|---|---|---|
| Ground | `#1B1B1F` | Window background (Mica, dark) |
| Session area | `#121215` | Around the letterboxed session |
| Bars and pages | `#202024` | Status bar, settings content |
| Cards | `#2A2A30` | Settings rows, dialogs `#2B2B30` |
| Lines | `rgba(255,255,255,0.06)`, `#3A3A44` | Separators, control borders |
| Text | `#F3F3F5`, secondary `#A8A8B3` / `#9A9AA6` | |
| Agent | `#8B7CFF`, on dark text `#B7AAFF` | The agent: accent colour, activity, command names, the session frame and the overlay |
| User | `#FFB547` | The user in control |
| Stop | `#C42B1C` | Stop, destructive buttons |
| Success / failure | `#4CC38A` / `#FF8A80` | Step results |
| Strings | `#E6C589` | Quoted text arguments |
| Fonts | Segoe UI Variable; Cascadia Mono for arguments, keys, paths and output | |

The overlay's cursor and highlights use the agent colour (today they are pink: change them).

## Window

- **Title bar**, drawn by the window: the app mark, name and version; then the toolbar: **Take control** (becomes **Give back control**, amber, while the user has it), **Stop** (disabled while the user has control), the display (resolution and scale, a dropdown), **Reset session**, **Settings**; then the system's caption buttons.
- **Session**: letterboxed; resizing the window never resizes the session. Its frame shows who is in control: violet while the agent works, amber while the user has control.
- **Interactive mode**: a notice at the top of the session ("You're in control", what goes where, Give back). The view runs at the host screen's refresh rate so that it feels native (Findings.md, open questions); while the agent is in control this matters less.

## Status bar

From left to right:

1. **Activity**: animated bars while a sequence runs, with the agent's description of the sequence (a `description` on `Run-Sequence`; "Agent working" without one) and "step x of y". Between sequences the bars slow down and dim for a few seconds ("Agent thinking"), then go quiet ("Agent idle"). While the user has control: "You're in control" in amber.
2. **The current action**: a spinner while it runs, then its result (check, cross); the action name in the agent colour; its arguments as rectangles, the name dim and the value bright (strings in quotes, sensitive text as dots); keys as keycaps (`Ctrl` + `Shift` + `Esc`). It brightens briefly when it changes, then settles. A failure follows as "failed: reason" in the failure colour. Durations read "60 s", "200 ms".
3. **Session**: state, frame rate, clipboard sync while the user has control.
4. **Log**: a button with the entry count (and a dot after a failure) that opens the log flyout.

## Log flyout

Expands over the window from its status bar button, and can expand to the window's height.

- Filters (All, Failed, Captures, Commands) and search.
- One group per sequence, titled by the agent's description, or by a title made from its steps without one ("Focus, click, type and 6 more"), never "Sequence 14"; then its number, step count, duration and state.
- Each step: its time in the sequence, result, name and arguments as in the status bar, and its duration.
- **Captures** show their image, size, and the file with Open and Copy path; inline captures are saved to a temporary folder as well, so the log can show them.
- **Commands** show their output live while they run (standard output and error), with Copy.
- A failed step shows its reason; sensitive text stays hidden and clipboard content short.

## Settings

Pages: **General** (session display and sign-in timeout, captures, default shell, password and setup), **Add to AI apps** (each client with its state: added, out of date, not added, not found; a snippet for other apps), **About**.

## Dialogs

- **Existing session**: a child session from an earlier run is still signed in. Resume, Reset or Cancel.
- **Closing while the agent works**: stops the sequence and signs the session out. Close and sign out, or Keep open.
- **Integration preview**: the change to an AI client's configuration file before it is written, with the backup's name and whether to restart the client.
- **One-time setup**: what is missing (child sessions not enabled), that it needs administrator rights once; an option "Smooth session view (60 fps)", on, with a line on what it changes machine-wide. Set up, or Not now.
