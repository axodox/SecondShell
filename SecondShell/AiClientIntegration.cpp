#include "pch.h"
#include "AiClientIntegration.h"
#include <shlobj.h>

using namespace std;
using namespace std::filesystem;

namespace SecondShell::Integrations
{
  namespace
  {
    path KnownFolder(const KNOWNFOLDERID& id)
    {
      PWSTR folder = nullptr;
      winrt::check_hresult(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &folder));
      path result{ folder };
      CoTaskMemFree(folder);
      return result;
    }
  }

  UserFolders UserFolders::Current()
  {
    UserFolders folders{ KnownFolder(FOLDERID_Profile), KnownFolder(FOLDERID_RoamingAppData), KnownFolder(FOLDERID_LocalAppData), KnownFolder(FOLDERID_ProgramFilesX86) };

    wchar_t codexHome[MAX_PATH];
    if (GetEnvironmentVariableW(L"CODEX_HOME", codexHome, MAX_PATH)) folders.CodexHome = codexHome;
    return folders;
  }

  void WriteConfigFile(const path& file, string_view text)
  {
    create_directories(file.parent_path());
    if (exists(file)) copy_file(file, path{ file } += ".bak", copy_options::overwrite_existing);

    auto temporary = path{ file } += ".secondshell.tmp";
    {
      ofstream stream{ temporary, ios::binary | ios::trunc };
      stream << text;
      if (!stream) throw runtime_error(format("Could not write {}.", winrt::to_string(file.wstring())));
    }
    rename(temporary, file);
  }

  string ReadConfigFile(const path& file)
  {
    error_code error;
    if (!exists(file, error) && !error) return {};

    ifstream stream{ file, ios::binary };
    if (!stream) throw runtime_error(format("Could not read {}.", winrt::to_string(file.wstring())));
    return { istreambuf_iterator<char>(stream), istreambuf_iterator<char>() };
  }
}
