#pragma once
#include "CredentialPrompt.h"
#include "CredentialStore.h"
#include "RdpHostWindow.h"

namespace SecondShell::Session
{
  //The user's Windows credentials for signing in to the child session (Design.md, Credentials): the saved ones, or else
  //the ones the user enters in the credential prompt. Entered credentials are saved only after they worked, saved ones
  //are deleted when the session rejects them, and a password is kept in memory only until its sign-in ends.
  class CredentialProvider
  {
    inline static const Axodox::Infrastructure::logger _logger{ "CredentialProvider" };

  public:
    explicit CredentialProvider(Axodox::Infrastructure::dependency_container* container);

    //The window that owns the credential prompt: SecondShell's main window.
    void SetPromptOwner(HWND owner);

    //On the RDP thread, before connecting. Asks for the credentials if none are saved; throws if the user cancels.
    void ApplyTo(RdpHostWindow& window);

    void OnSignInSucceeded();

    //The sign-in did not complete: the connection ended with the RDP control's disconnect reason, or 0 when the sign-in
    //was cancelled or timed out.
    void OnSignInFailed(long disconnectReason);

  private:
    std::shared_ptr<CredentialStore> _store;
    CredentialPrompt _prompt;
    std::atomic<HWND> _promptOwner = nullptr;

    std::optional<Credentials> _entered;  //from the prompt, while signing in with them
    bool _wasRejected = false;

    Credentials Ask();
  };
}
