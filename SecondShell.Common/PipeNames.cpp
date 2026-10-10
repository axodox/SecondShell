#include "pch.h"
#include "PipeNames.h"

using namespace std;

namespace SecondShell::Environment
{
  namespace
  {
    string CurrentUserSid()
    {
      winrt::handle token;
      winrt::check_bool(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()));

      DWORD size = 0;
      GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);

      vector<uint8_t> buffer(size);
      winrt::check_bool(GetTokenInformation(token.get(), TokenUser, buffer.data(), size, &size));

      char* sid = nullptr;
      winrt::check_bool(ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid));
      string result{ sid };
      LocalFree(sid);
      return result;
    }

    string PipeName(string_view purpose)
    {
      static const auto sid = CurrentUserSid();
      return format("SecondShell.{}.{}", sid, purpose);
    }
  }

  string PipeNames::Driver()
  {
    return PipeName("Driver");
  }

  string PipeNames::Mcp()
  {
    return PipeName("Mcp");
  }
}
