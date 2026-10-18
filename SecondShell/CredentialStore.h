#pragma once
#include "Credentials.h"

namespace SecondShell::Session
{
  //The saved credentials of the child session: a generic Credential Manager entry, "SecondShell:ChildSession" (never an
  //RDP target name such as termsrv/*, Findings.md 1). Its blob is also encrypted with DPAPI (user scope, SecondShell's
  //entropy), so reading the entry, e.g. with cmdkey or a script, does not reveal the password.
  class CredentialStore
  {
    inline static const Axodox::Infrastructure::logger _logger{ "CredentialStore" };

  public:
    CredentialStore();
    explicit CredentialStore(std::wstring targetName);

    //Nothing if none are saved, or if the entry cannot be decrypted (e.g. it was not written by SecondShell).
    std::optional<Credentials> Load() const;
    void Save(const Credentials& credentials);
    void Delete();

  private:
    std::wstring _targetName;
  };
}
