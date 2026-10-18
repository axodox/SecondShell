# Stage 1 – Risk spikes: test plan

Stage 1 of [ImplementationPlan.md](ImplementationPlan.md) checks the assumptions that could still change the design before the product code depends on them. This document describes the checks so that anyone can carry them out: what each one answers, how to run it, what to measure, and what counts as a pass.

The spikes are throwaway programs in `Spikes\<SpikeName>`, not product code. Results go into [Findings.md](Findings.md), and the design documents are updated before the stages that depend on them (5, 7, 8) start.

## Common setup and rules

**Machine**: Windows 11 Pro or Enterprise, child sessions enabled (see Findings.md 2.2 for the saved-password sign-in). Each spike also notes whether it should be repeated on an ARM64 device or VM.

**Getting a program into the child session**: as in Findings.md 2.8. Write a `RunOnce` entry for the test program, connect to a new child session, and Windows starts the program at sign-in. `Spikes/Findings/runkeytest.ps1` shows the connection side and `SsCapture.cs` the program side.

**Rules for every spike**:

- **Session guard first.** Any test program that injects input or captures the screen must first check that it runs in the child session the tester expects (e.g. compare its session ID with an expected ID written to a file before connecting), and exit immediately otherwise. Findings.md 2.7 shows what happens without it.
- **Input only in the child session.** Never run an injecting test program in the user's own session.
- **Clean up.** After each run, log the child session off explicitly (`WTSLogoffSession`), remove any `RunOnce` entries and test registrations, and check with `qwinsta` that no child session is left.
- **Shared profile.** Apps in the child session see the user's real app data (Findings.md 2.10). Use test files in a scratch folder, and avoid apps whose state the user cares about.
- **Record versions**: Windows build, GPU and driver, Windows App SDK version.

## Spike 1 – RDP control in a hidden window, from a WinUI 3 process

**Question**: Can the WinUI 3 host process own the RDP ActiveX control in a separate, hidden Win32 window, connect to the child session with it, and keep the session working while the control is never shown?

**Why it matters**: Design.md (Session display) puts the RDP control in a hidden window and shows the session through captured frames. Findings.md 2.5 verified hidden windows with a WinForms test host; this checks the real host's process type