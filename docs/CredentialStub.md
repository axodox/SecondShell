# Credential stub – what is missing

`SecondShell\CredentialProvider` (host, namespace `SecondShell::Session`) is the only class that handles the user's Windows credentials. It is a stub: `ApplyTo` throws, so `Initialize-Session` fails with "Signing in to the child session is not implemented yet". Everything around it (RDP control, driver start, session guard, sequencing) is implemented.

## The contract

- `ApplyTo(IUnknown* rdpControl)`: called on the RDP thread after the control is configured and before it connects. Sets the user name, domain and password on the control (MsRdpClient9NotSafeForScripting: `UserName`, `Domain`, and the advanced settings' clear-text password). Throws if no credentials are available.
- `OnSignInSucceeded()`: the session signed in with them.
- `OnSignInFailed(long disconnectReason)`: the connection ended before sign-in completed; the reason is the RDP control's disconnect reason.

## What to implement (Design.md, Credentials)

1. **Load** the saved credentials from Windows Credential Manager: a generic credential named `SecondShell:ChildSession` (never `termsrv/*` or `localhost`), whose blob is additionally encrypted with DPAPI (user scope, application-specific entropy).
2. **Prompt** when none are saved: the standard Windows credential prompt (`CredUIPromptForWindowsCredentials`), owned by the SecondShell window and shown in the foreground. `ApplyTo` runs on the RDP thread, so the prompt needs the main window's handle.
3. **Save** newly entered credentials only in `OnSignInSucceeded`.
4. **Delete** saved credentials in `OnSignInFailed` when the reason means the password was rejected (e.g. after a password change), so the next start prompts again. Other disconnect reasons keep them.
5. **Handle with care**: never log the password or pass it on a command line; clear it from memory after use.
6. **Forget**: later, a "Forget saved password" command (settings page) and the uninstaller remove the entry.

## Testing

The first sign-in is a manual test: enter the password in the prompt. Later runs use the saved credentials, so the MCP probe can run the end-to-end tests unattended.
