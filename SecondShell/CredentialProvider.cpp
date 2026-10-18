#include "pch.h"
#include "CredentialProvider.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    //Disconnect reasons (IMsTscAxEvents::OnDisconnected) for a password the session does not accept: logon failed (seen
    //with a wrong or missing password, Findings.md 2.2), password expired, password must be changed.
    const unordered_set<long> _passwordRejectedReasons{ 2055, 3847, 4615 };

    const wchar_t* const _askMessage = L"SecondShell signs in to the agent desktop, a child session of your Windows account, with your Windows password. It is saved, encrypted, once the sign-in succeeds.";
    const wchar_t* const _askAgainMessage = L"Windows rejected the password, e.g. because it was changed. Enter your current Windows password to sign in to the agent desktop.";

    //The control takes the user name and the domain separately
    pair<wstring, wstring> SplitUserName(const wstring& userName)
    {
      wchar_t user[CREDUI_MAX_USERNAME_LENGTH + 1]{}, domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1]{};
      if (CredUIParseUserNameW(userName.c_str(), user, ARRAYSIZE(user), domain, ARRAYSIZE(domain)) != NO_ERROR) return { userName, L"" };
      return { user, domain };
    }
  }

  CredentialProvider::CredentialProvider(dependency_container* container) :
    _store(container->resolve<CredentialStore>())
  { }

  void CredentialProvider::SetPromptOwner(HWND owner)
  {
    _promptOwner = owner;
  }

  void CredentialProvider::ApplyTo(RdpHostWindow& window)
  {
    _entered.reset();
    auto saved = _store->Load();
    if (!saved) _entered = Ask();

    auto& credentials = saved ? *saved : *_entered;
    auto [userName, domain] = SplitUserName(credentials.UserName);
    window.SetCredentials(userName, domain, credentials.Password.View());

    _logger.log(log_severity::information, "Signing in as {} with {} credentials.", winrt::to_string(credentials.UserName), saved ? "saved" : "entered");
  }

  void CredentialProvider::OnSignInSucceeded()
  {
    _wasRejected = false;
    if (!_entered) return;

    try
    {
      _store->Save(*_entered);
      _logger.log(log_severity::information, "Saved the credentials.");
    }
    catch (const winrt::hresult_error& error)
    {
      _logger.log(log_severity::warning, "The credentials could not be saved, so they will be asked for again: {}", winrt::to_string(error.message()));
    }

    _entered.reset();
  }

  void CredentialProvider::OnSignInFailed(long disconnectReason)
  {
    _entered.reset();
    _wasRejected = _passwordRejectedReasons.contains(disconnectReason);
    if (!_wasRejected) return;

    _logger.log(log_severity::warning, "The child session rejected the password (disconnect reason {}); it will be asked for again.", disconnectReason);
    try
    {
      _store->Delete();
    }
    catch (const winrt::hresult_error& error)
    {
      _logger.log(log_severity::error, "The rejected credentials could not be deleted: {}", winrt::to_string(error.message()));
    }
  }

  Credentials CredentialProvider::Ask()
  {
    _logger.log(log_severity::information, "No saved credentials; showing the credential prompt.");

    auto credentials = _prompt.Show(_promptOwner, _wasRejected ? _askAgainMessage : _askMessage);
    if (!credentials) throw runtime_error("Signing in to the child session needs your Windows password, and the credential prompt was cancelled.");
    return move(*credentials);
  }
}
