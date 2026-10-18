#pragma once

namespace SecondShell::Integrations
{
  named_enum(IntegrationState,
    NotFound,    //the app is not on this PC
    NotAdded,
    Added,
    Outdated,    //its entry runs another SecondShell.Mcp.exe, e.g. from a folder the portable build moved from
    Unreadable   //its config file could not be read, e.g. it is not valid JSON
  );

  //What adding or removing the entry changes, shown before it is done (docs/UIDesign.md, Integration preview).
  struct IntegrationChange
  {
    std::filesystem::path File;    //empty for a change made by the app's own command line
    std::string Before;            //the file's text, or the command line
    std::string After;
    std::string Note;              //e.g. "Restart Cursor afterwards."
  };

  //The folders the apps keep their configuration in; other folders in tests.
  struct UserFolders
  {
    std::filesystem::path Profile;       //%USERPROFILE%
    std::filesystem::path RoamingData;   //%APPDATA%
    std::filesystem::path LocalData;     //%LOCALAPPDATA%
    std::filesystem::path ProgramsX86;   //%ProgramFiles(x86)%
    std::filesystem::path CodexHome;     //%CODEX_HOME% if set, else empty

    static UserFolders Current();
  };

  //One AI app: whether it is there, whether SecondShell is in its MCP configuration, and adding, updating or removing
  //that entry (Design.md, AI client integration). Registered in a registry<AiClientIntegration>.
  class AiClientIntegration
  {
  public:
    virtual ~AiClientIntegration() = default;

    virtual std::string_view Id() const = 0;    //for --integrate, e.g. "cursor"
    virtual std::string_view Name() const = 0;  //"Cursor"
    virtual std::string Where() const = 0;      //"Its mcp.json in your profile"

    virtual IntegrationState State() const = 0;
    virtual IntegrationChange PreviewAdd() const = 0;
    virtual IntegrationChange PreviewRemove() const = 0;

    //Adds the entry, or points it at this SecondShell; throws with what went wrong.
    virtual void Add() = 0;
    virtual void Remove() = 0;
  };

  //Writes a config file, a backup of it first (<file>.bak), through a temporary file, so a failed write leaves it as it was.
  void WriteConfigFile(const std::filesystem::path& file, std::string_view text);
  //Empty if there is none; throws if it is there but cannot be read, so it is never taken for an empty one and replaced.
  std::string ReadConfigFile(const std::filesystem::path& file);
}
