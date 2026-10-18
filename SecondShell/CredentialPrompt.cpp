#include "pch.h"
#include "CredentialPrompt.h"

using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    const wchar_t* const _caption = L"SecondShell";

    wstring CurrentUserName()
    {
      wchar_t name[CREDUI_MAX_USERNAME_LENGTH + 1];
      ULONG length = ARRAYSIZE(name);
      winrt::check_bool(GetUserNameExW(NameSamCompatible, name, &length));
      return name;
    }

    vector<uint8_t> PackUserName(const wstring& userName)
    {
      auto name = const_cast<wchar_t*>(userName.c_str());
      auto noPassword = const_cast<wchar_t*>(L"");

      DWORD size = 0;
      CredPackAuthenticationBufferW(CRED_PACK_GENERIC_CREDENTIALS, name, noPassword, nullptr, &size);
      vector<uint8_t> buffer(size);
      winrt::check_bool(CredPackAuthenticationBufferW(CRED_PACK_GENERIC_CREDENTIALS, name, noPassword, buffer.data(), &size));
      return buffer;
    }

    optional<Credentials> Unpack(void* buffer, ULONG size)
    {
      wchar_t userName[CREDUI_MAX_USERNAME_LENGTH + 1], domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1], password[CREDUI_MAX_PASSWORD_LENGTH + 1];
      DWORD userNameLength = ARRAYSIZE(userName), domainLength = ARRAYSIZE(domain), passwordLength = ARRAYSIZE(password);

      optional<Credentials> result;
      if (CredUnPackAuthenticationBufferW(CRED_PACK_GENERIC_CREDENTIALS, buffer, size, userName, &userNameLength, domain, &domainLength, password, &passwordLength))
      {
        auto fullUserName = *domain ? format(L"{}\\{}", domain, userName) : wstring(userName);
        result = Credentials{ move(fullUserName), Secret{ password } };
      }

      SecureZeroMemory(password, sizeof(password));
      return result;
    }

    void BringToFront(HWND owner)
    {
      if (!owner || SetForegroundWindow(owner)) return;

      FLASHWINFO flash{ .cbSize = sizeof(flash), .hwnd = owner, .dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG };
      FlashWindowEx(&flash);
    }
  }

  optional<Credentials> CredentialPrompt::Show(HWND owner, wstring_view message) const
  {
    BringToFront(owner);

    wstring messageText{ message };
    CREDUI_INFOW info{ .cbSize = sizeof(info), .hwndParent = owner, .pszMessageText = messageText.c_str(), .pszCaptionText = _caption };
    auto userName = PackUserName(CurrentUserName());

    ULONG package = 0;
    void* entered = nullptr;
    ULONG enteredSize = 0;
    auto result = CredUIPromptForWindowsCredentialsW(&info, 0, &package, userName.data(), ULONG(userName.size()), &entered, &enteredSize, nullptr, CREDUIWIN_GENERIC);
    if (result == ERROR_CANCELLED) return nullopt;
    winrt::check_win32(result);

    auto credentials = Unpack(entered, enteredSize);
    SecureZeroMemory(entered, enteredSize);
    CoTaskMemFree(entered);

    if (!credentials) throw runtime_error("The credentials entered in the Windows credential prompt could not be read.");
    return credentials;
  }
}
