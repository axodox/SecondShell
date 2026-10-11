#include "pch.h"
#include "CredentialProvider.h"

using namespace std;

namespace SecondShell::Session
{
  void CredentialProvider::ApplyTo(IUnknown* /*rdpControl*/)
  {
    throw runtime_error("Signing in to the child session is not implemented yet (CredentialProvider is a stub).");
  }

  void CredentialProvider::OnSignInSucceeded()
  { }

  void CredentialProvider::OnSignInFailed(long /*disconnectReason*/)
  { }
}
