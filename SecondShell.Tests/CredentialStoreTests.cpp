#include "pch.h"
#include "CredentialStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Tests
{
  namespace
  {
    //An entry of the tests' own, never the child session's
    const wchar_t* const _testTarget = L"SecondShell:Tests.CredentialStore";
    const wchar_t* const _testUserName = L"TEST-PC\\tester";
    const wstring_view _testPassword = L"not-a-real-password";

    Credentials TestCredentials()
    {
      return { _testUserName, Secret{ _testPassword } };
    }

    vector<uint8_t> ReadStoredBlob()
    {
      PCREDENTIALW entry;
      Assert::IsTrue(CredReadW(_testTarget, CRED_TYPE_GENERIC, 0, &entry));
      vector<uint8_t> blob(entry->CredentialBlob, entry->CredentialBlob + entry->CredentialBlobSize);
      CredFree(entry);
      return blob;
    }

    void WriteUnencryptedEntry()
    {
      CREDENTIALW entry{
        .Type = CRED_TYPE_GENERIC,
        .TargetName = const_cast<wchar_t*>(_testTarget),
        .CredentialBlobSize = DWORD(_testPassword.size() * sizeof(wchar_t)),
        .CredentialBlob = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(_testPassword.data())),
        .Persist = CRED_PERSIST_SESSION,
        .UserName = const_cast<wchar_t*>(_testUserName)
      };
      Assert::IsTrue(CredWriteW(&entry, 0));
    }
  }

  TEST_CLASS(CredentialStoreTests)
  {
    CredentialStore _store{ _testTarget };

  public:
    TEST_METHOD_CLEANUP(DeleteTestEntry)
    {
      _store.Delete();
    }

    TEST_METHOD(LoadReturnsSavedCredentials)
    {
      _store.Save(TestCredentials());

      auto loaded = _store.Load();
      Assert::IsTrue(loaded.has_value());
      Assert::AreEqual(_testUserName, loaded->UserName.c_str());
      Assert::IsTrue(loaded->Password.View() == _testPassword);
    }

    TEST_METHOD(StoredEntryDoesNotContainThePassword)
    {
      _store.Save(TestCredentials());

      auto blob = ReadStoredBlob();
      auto password = reinterpret_cast<const uint8_t*>(_testPassword.data());
      Assert::IsTrue(search(blob.begin(), blob.end(), password, password + _testPassword.size() * sizeof(wchar_t)) == blob.end());
    }

    TEST_METHOD(LoadReturnsNothingWhenNoneAreSaved)
    {
      Assert::IsFalse(_store.Load().has_value());
    }

    TEST_METHOD(DeleteRemovesSavedCredentials)
    {
      _store.Save(TestCredentials());
      _store.Delete();

      Assert::IsFalse(_store.Load().has_value());
    }

    TEST_METHOD(DeleteSucceedsWhenNoneAreSaved)
    {
      _store.Delete();
    }

    TEST_METHOD(LoadIgnoresEntriesNotSavedBySecondShell)
    {
      WriteUnencryptedEntry();

      Assert::IsFalse(_store.Load().has_value());
    }
  };
}
