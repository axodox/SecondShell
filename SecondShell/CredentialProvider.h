#pragma once

namespace SecondShell::Session
{
  //The user's Windows credentials for signing in to the child session: the only class that handles them.
  //
  //STUB: not implemented yet. The design (Design.md, Credentials): the password is asked for with the Windows
  //credential prompt (CredUIPromptForWindowsCredentials, owned by the SecondShell window), stored in Credential Manager
  //as a generic credential "SecondShell:ChildSession" with a DPAPI-encrypted blob, saved only after a successful
  //sign-in, deleted when the session rejects it, never logged, and cleared from memory after use.
  class CredentialProvider
  {
  public:
    //Sets the user name, domain and password on the RDP control (MsRdpClient9NotSafeForScripting) before it connects.
    //Throws if there are no credentials.
    void ApplyTo(IUnknown* rdpControl);

    void OnSignInSucceeded();

    //The connection ended before sign-in completed, with the RDP control's disconnect reason; e.g. a wrong password.
    void OnSignInFailed(long disconnectReason);
  };
}
