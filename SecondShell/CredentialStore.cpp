#include "pch.h"
#include "CredentialStore.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    const wchar_t* const _childSessionTarget = L"SecondShell:ChildSession";
    const wchar_t* const _comment = L"Signs SecondShell in to its child session. The password is encrypted for SecondShell only.";

    //Ties the encrypted password to SecondShell: other programs of the user cannot decrypt it with DPAPI alone
    const unsigned char _entropy[] = "SecondShell.ChildSession.Password.v1";

    DATA_BLOB Blob(span<const uint8_t> bytes)
    {
      return { DWORD(bytes.size()), const_cast<BYTE*>(bytes.data()) };
    }

    DATA_BLOB Entropy()
    {
      return Blob({ _entropy, sizeof(_entropy) - 1 });
    }

    vector<uint8_t> Encrypt(wstring_view text)
    {
      auto input = Blob({ reinterpret_cast<const uint8_t*>(text.data()), text.size() * sizeof(wchar_t) });
      auto entropy = Entropy();
      DATA_BLOB output{};
      winrt::check_bool(CryptProtectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output));

      vector<uint8_t> result(output.pbData, output.pbData + output.cbData);
      LocalFree(output.pbData);
      return result;
    }

    optional<Secret> Decrypt(span<const uint8_t> encrypted)
    {
      auto input = Blob(encrypted);
      auto entropy = Entropy();
      DATA_BLOB output{};
      if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return nullopt;

      Secret result{ { reinterpret_cast<const wchar_t*>(output.pbData), output.cbData / sizeof(wchar_t) } };
      SecureZeroMemory(output.pbData, output.cbData);
      LocalFree(output.pbData);
      return result;
    }
  }

  CredentialStore::CredentialStore() :
    CredentialStore(_childSessionTarget)
  { }

  CredentialStore::CredentialStore(wstring targetName) :
    _targetName(move(targetName))
  { }

  optional<Credentials> CredentialStore::Load() const
  {
    PCREDENTIALW entry;
    if (!CredReadW(_targetName.c_str(), CRED_TYPE_GENERIC, 0, &entry))
    {
      if (GetLastError() == ERROR_NOT_FOUND) return nullopt;
      winrt::throw_last_error();
    }

    unique_ptr<CREDENTIALW, decltype(&CredFree)> owner{ entry, &CredFree };
    auto password = Decrypt({ entry->CredentialBlob, entry->CredentialBlobSize });
    if (!password || !entry->UserName)
    {
      _logger.log(log_severity::warning, "The saved credentials cannot be decrypted, e.g. they were not saved by SecondShell.");
      return nullopt;
    }

    return Credentials{ entry->UserName, move(*password) };
  }

  void CredentialStore::Save(const Credentials& credentials)
  {
    auto blob = Encrypt(credentials.Password.View());
    CREDENTIALW entry{
      .Type = CRED_TYPE_GENERIC,
      .TargetName = _targetName.data(),
      .Comment = const_cast<wchar_t*>(_comment),
      .CredentialBlobSize = DWORD(blob.size()),
      .CredentialBlob = blob.data(),
      .Persist = CRED_PERSIST_LOCAL_MACHINE,
      .UserName = const_cast<wchar_t*>(credentials.UserName.c_str())
    };
    winrt::check_bool(CredWriteW(&entry, 0));
  }

  void CredentialStore::Delete()
  {
    if (!CredDeleteW(_targetName.c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) winrt::throw_last_error();
  }
}
