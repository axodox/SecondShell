# Test scripts behind Findings.md

The scripts that produced the results in [docs/Findings.md](../../docs/Findings.md), kept as they were run. They are a record, not tools: they contain absolute paths of the machine and folder they ran in (a temporary scratch folder), and expect `MSTSCLib.dll` / `AxMSTSCLib.dll` from the Windows Simulator next to them.

| Script | Findings section | What it does |
|---|---|---|
| `childtest.ps1` | 2.1 | Second RDP client with `ConnectToChildSession` while another is connected |
| `credtest.ps1` | 2.2 | Sign-in to the child session with a Credential Manager password |
| `keystest.ps1` | 2.6 | Starts a program in the child session by typing Win+R with `SendKeys` |
| `hiddentest.ps1` | 2.5 | Capture, frame freshness and `SendInput` with the RDP window visible, minimized, hidden, off-screen |
| `posttest.ps1` | 2.6, 2.7 | Posted keyboard messages with the RDP window hidden; COM activation with the session guard |
| `runkeytest.ps1` | 2.8 | Driver start through `Run` / `RunOnce` entries, without keystrokes or focus |
| `injhost.ps1`, `injtest.ps1` | 2.9, 2.10 | `InputInjector` (unpackaged) in the child session: Unicode typing into Notepad, virtual keys, mouse |
| `SsCapture.cs` | 2.5, 2.7, 2.8 | Test program run inside the child session: session guard, capture loop, counter window, `SendInput` |

All scripts that inject input do so only inside the child session, behind a session guard (`expected_session.txt`).