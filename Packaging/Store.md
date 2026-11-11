# Microsoft Store submission

How SecondShell's MSIX package goes to the Microsoft Store, and what to tell certification. The sideloaded package
(`package.ps1`, self-signed, GitHub releases) stays as it is; the Store's upload is the same package with the identity
Partner Center reserves, unsigned (the Store signs it).

## Steps

1. **Reserve the name** in Partner Center (Apps and games, New product, MSIX or PWA app): `SecondShell`.
2. **Copy the identity** from Product management, Product identity:
   - `Package/Identity/Name` (like `12345Axodox.SecondShell`)
   - `Package/Identity/Publisher` (like `CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`)
   - `Package/Properties/PublisherDisplayName` (like `Axodox`)
3. **Make the upload**, either:
   - in CI: set the AppVeyor project's variables `SECONDSHELL_STORE_NAME`, `SECONDSHELL_STORE_PUBLISHER` and
     `SECONDSHELL_STORE_PUBLISHER_DISPLAY_NAME` (not secret); every build then has the artifact `Store`,
     `SecondShell-<version>.msixupload`;
   - or locally: `.\package.ps1 -StoreName ... -StorePublisher "CN=..." -StorePublisherDisplayName ...`, which writes
     `artifacts\store\SecondShell-<version>.msixupload`.
4. **Raise the version for every submission**: the Store's packages are versioned `<SecondShellVersion>.0` (the Store
   keeps the fourth part for itself, so CI's build number is not in it), and each submission must be higher than the
   last: raise `SecondShellVersion` in `Build\SecondShell.Cpp.props`.
5. **Submit**: Packages, upload the `.msixupload` (both platforms' packages and their symbols); Store listing,
   properties, age ratings; the restricted capabilities and the notes for certification below.

## Before the first submission

- **unvirtualizedResources** may be refused (see below). If it is, the choice is between changing the design (the
  data folder and the AI apps' configuration files would have to be written outside the package's virtualization, by
  a process outside the package) or listing SecondShell in the Store as a Win32 app (an `.exe` or `.msi` installer
  hosted by us, which the Store takes since 2021 without MSIX's restrictions; SecondShell has no installer yet, only
  the portable zip).
- **System requirements**: SecondShell needs child sessions (the Remote Desktop stack). Whether they work on Windows
  Home is not verified; if not, the listing's requirements must say Pro, Enterprise or Education.
- **Privacy policy**: needed, as SecondShell captures the screen and keeps the user's sign-in password (in Windows
  Credential Manager, for the child session's sign-in). It sends nothing anywhere itself; the captures and the
  processes' output go to the AI app the user connects, on this PC.
- **Test on a clean PC** from the Store's flighting (a private audience) before going public: the elevated setup
  (enabling child sessions) from the Store's package, and the startup task starting the driver.

## WACK (Windows App Certification Kit)

Run on `SecondShell-0.1.0-x64.msix` (0.1.0.7, 2026-11-11), elevated:
`appcert.exe test -appxpackagepath <msix> -reportoutputpath <xml>`. Overall result: **PASS**, with two tests failing
that do not fail the kit:

- **Application count**: the package has three applications (the window, the MCP server and the driver), as each needs
  its own App Execution Alias (one application may declare only one). Other Store packages do the same (Python's has
  several). Explain it in the notes for certification.
- **Blocked executables**: SecondShell starts processes (`CreateProcess`, `ShellExecute`) and mentions `cmd.exe` and
  `powershell.exe`, as running the agent's commands is what it is for. This test matters only for Windows in S mode,
  where SecondShell does not work anyway; leave S mode out in the listing's requirements if Partner Center asks.

Fixed for it: the MCP server is per-monitor DPI aware (it was flagged), and the packages leave out the `.winmd` files
(the WinUI ones duplicate Windows' own GUIDs, which failed; C++ programs do not read them at run time).

## Restricted capabilities: justifications

Partner Center asks why each restricted capability is needed. Texts to paste:

**runFullTrust**: SecondShell is a desktop app (WinUI 3, C++) that runs at medium integrity, like any desktop app. It
starts a second Windows session of the same user on the same PC (a Remote Desktop child session), runs a helper in it
that injects mouse, keyboard, touch and pen input and captures the screen, and talks to its MCP server over named
pipes. None of this is possible in an AppContainer.

**unvirtualizedResources**: SecondShell is a tool for AI apps that are not packaged (Claude Code, VS Code, Codex and
others). The files it writes are read by those apps, outside the package, so they must be in their real locations:

- Screen captures and processes' output are files in `%LOCALAPPDATA%\SecondShell`, whose paths SecondShell returns to
  the AI app, which then opens them. Virtualized, the AI app would get a path that does not exist for it.
- "Add to AI apps" adds SecondShell's MCP server to the AI apps' own configuration files, some of them under
  `%APPDATA%` (for example Claude Desktop's `%APPDATA%\Claude\claude_desktop_config.json`). Virtualized, the AI app
  would never see the entry.
- The child session's helper is started by a `RunOnce` entry in `HKCU` when the startup task is off, which Explorer in
  the child session must see.

Everything SecondShell writes this way is its own: its data folder, its entries in the AI apps' configuration (shown
to the user before they are written, with a backup, and removed by its uninstall command) and its own `RunOnce` entry,
removed when used.

## Notes for certification

Suggested text:

> SecondShell gives AI agents a second Windows desktop: it signs the current user in to a second session (a Remote
> Desktop child session) on the same PC, where the agent works through SecondShell's MCP server without disturbing
> the user. To test: start SecondShell; if child sessions are not enabled, it offers to enable them (one administrator
> prompt), then asks for a restart. After the restart, Start session shows the second desktop in the window; Settings,
> Add to AI apps connects an AI app (for example Claude Code or VS Code) to it. The package has three applications,
> each with its App Execution Alias: SecondShell.exe (the window), SecondShell.Mcp.exe (the MCP server, which AI apps
> start) and SecondShell.Driver.exe (started in the second session by the package's startup task). It starts processes
> on the agent's request, as its purpose is running the agent's tools; it does not support Windows in S mode.
