# Todo

Features to consider; not yet implemented.

## Capture Screen button

A **Capture Screen** button in the title-bar toolbar, between **Take control** and **Stop**. It takes a full-resolution PNG screen capture of the session display and puts it on the clipboard.

## Working while the screen is locked

Investigate whether capture/control can be made to work while the machine's screen is locked for a long time — it currently returns access denied. Find out what access the capture and input paths lose at the lock screen and whether there is a supported way around it.

Seen in an agent session (Ridge plugin work, 19–29 October): mouse and keyboard actions failed with `ActionFailed` "Access is denied" 10 times, during six separate absences of the user. Captures and `Process.Run` kept working; only input failed. `Initialize-Session` with `Reset` did not help. Even where it can't be made to work:

- Give it an error code of its own (e.g. `InputUnavailable`) instead of a generic `ActionFailed`. `Check-Session` reported `Ready` with the driver connected the whole time, so the agent only found out when a click failed; it should report whether input is available.
- Let the agent know when input works again, instead of it retrying.
- Try whether UI Automation actions (invoke, toggle or set value on an element found by name) still work while locked, as they don't go through `SendInput`.

## Sequences longer than the client's timeout

17 `Run-Sequence` calls failed with the client's "Request timed out": every one whose waits added up to about 60 seconds or more. The sequence's own `timeout` (90–180 s) made no difference; the longest call that succeeded waited 57 s. The limit is the MCP client's request timeout (in Claude Code, probably `MCP_TOOL_TIMEOUT`), and progress is only reported between steps, so one long `Control.Wait` sends nothing.

- Say the limit in the tool description, and reject sequences whose fixed waits exceed it with a clear message.
- Send progress during long steps, for clients that reset their timeout on progress.
- Or let a sequence run in the background: return an id, and collect the result with a second tool.
- Check that a sequence the client gave up on is cancelled in the host, so its later clicks don't still happen.

## Launching packaged apps

There is no action to start a packaged app. The agent went through `explorer.exe shell:appsFolder\<AUMID>`, which worked at first but later only opened Explorer's Home window, and through `Start-Process`/`cmd start`. Starting through the app's execution alias needs `workingDirectory` set to the package folder, or apps that load files by relative path fail (the Ridge client crashed at startup). Add an action that activates an app by its AUMID with `IApplicationActivationManager`, with optional arguments, and returns its PID.

## `Process.Run` arguments with a shell

The default shell is PowerShell, and `args` become the script block's `$args`, so `"command": "cmd.exe", "args": ["/c", ...]` started cmd interactively and ignored the arguments (its banner came back as stdout). When `args` are given, run the program itself, or reject `args` together with a shell.

## Output of asynchronous processes

An `async` process's console output isn't collected, so to find a line in a server's log the agent had to use Windows Terminal's search box through the UI. Add an option to write the output to a file, or a way to read it later.

## Prompts nobody answers in the session

Endpoint protection (Trend Micro Apex One) shows its "new program" prompts in the child session too. Nobody answers them there, so they time out and the program is blocked: starts of a newly built `colmap.exe` failed with "Access is denied". Report new windows of unrelated processes in the sequence results, and notify the user in the host.

## Reset of a running session

`Initialize-Session` with `"existingSession": "Reset"` on a session that was already running returned the same session id and driver PID 3 times out of 5, without saying it hadn't reset anything. Either reset the running session (log it off and start a new one) or say that it didn't.

## Session lost right after starting

Twice, `Initialize-Session` returned `Ready` with the driver connected, and the next `Run-Sequence` failed with `SessionNotActive`; `Check-Session` then showed the driver disconnected. Once, the session ended on its own within 25 minutes of a `Set-Display` change from 3840×2160 back to 1920×1080 (possibly unrelated).

## Waiting for control to come back

While the user has control, sequences fail with `AgentInputDisabled`, and the agent can only find out that control is back by retrying. Add a way to wait for it (an action, or a field in `Check-Session` with an event).

## Picking windows by title or process

Window actions take only a handle, which changes when an app restarts (`InvalidArgument` "There is no window ..."). The agent tried `title`, a title string and `processName` before falling back to `Window.Enumerate`. Accept a title, process name or PID as well.

## Argument names the agent guesses

About 54 of the 73 failed calls in that session were rejected arguments. The error messages list the valid properties, which helped, but only the first problem is reported, so one call took three round trips (`Wait`, then `milliseconds`, then `scale`). Most common:

| Mistake | Times |
|---|---|
| `Control.Wait` with `milliseconds` instead of `duration` | 7 |
| `Capture.Screen` with `scale` | 7 |
| an action named `Wait` | 4 |
| `Process.Run` with `arguments` | 3 |
| `Mouse.Press` with `x`/`y` | 3 |
| `Mouse.Drag`, `Mouse.TripleClick`, `Mouse.Scroll` | 2, 1, 1 |
| click modifiers; keys like `"Ctrl+A"`; modifier `"Control"` | 5 |

- Report every invalid step in one response.
- Accept common aliases (`milliseconds`, `Wait`, `Ctrl+A`, `Control`).
- Add `Mouse.Drag`, `Mouse.TripleClick`, modifiers on clicks, and a scale on captures.
- Put a few short examples in the tool description: agents rebuild calls from memory after their context is compacted.

## Frame rate in the session

FPS measured in the child session is capped by its compose rate (Ridge showed 36–41 FPS there; Findings.md 2.13). Mention it in the tool description so performance checks aren't misread.

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
