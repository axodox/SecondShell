#pragma once
#include "Credentials.h"

namespace SecondShell::Session
{
  //The Windows credential prompt (CredUIPromptForWindowsCredentials), filled in with the current user's name. Brought to
  //the front with its owner, or, where Windows does not allow that, the owner flashes on the taskbar (Findings.md 5).
  class CredentialPrompt
  {
  public:
    //Waits for the user; nothing if they cancelled.
    std::optional<Credentials> Show(HWND owner, std::wstring_view message) const;
  };
}
