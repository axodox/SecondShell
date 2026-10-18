#pragma once
#include "AiClientIntegration.h"
#include "JsonConfigEditor.h"

namespace SecondShell::Integrations
{
  //An app that keeps its MCP servers in a JSON file of its own.
  struct JsonFileClient
  {
    std::string Id;
    std::string Name;
    std::string Where;                         //for the page, e.g. "Its mcp.json in your profile"
    std::filesystem::path ConfigFile;
    std::string Section;                       //"mcpServers", or "servers" for VS Code and Visual Studio
    bool HasTypeField = false;                 //"type": "stdio" in the entry
    std::vector<std::filesystem::path> InstallMarkers;  //any of them there means the app is installed
    std::string RestartNote;
  };

  //Adds, updates and removes the "secondshell" entry in the app's file, changing nothing else in it.
  class JsonFileIntegration : public AiClientIntegration
  {
  public:
    JsonFileIntegration(JsonFileClient client, std::filesystem::path mcpExecutable);

    virtual std::string_view Id() const override;
    virtual std::string_view Name() const override;
    virtual std::string Where() const override;

    virtual IntegrationState State() const override;
    virtual IntegrationChange PreviewAdd() const override;
    virtual IntegrationChange PreviewRemove() const override;

    virtual void Add() override;
    virtual void Remove() override;

  private:
    JsonFileClient _client;
    std::filesystem::path _mcpExecutable;

    JsonEntry Entry() const;
    bool IsInstalled() const;

    //The name SecondShell is under: "secondshell", or the name of an entry added by hand that runs a
    //SecondShell.Mcp.exe, so it is updated rather than added twice.
    std::optional<std::string> EntryName(const JsonConfigEditor& editor) const;
  };

  //Whether an entry's "command" runs this SecondShell.Mcp.exe; JSON text of the entry.
  bool RunsExecutable(std::string_view entry, const std::filesystem::path& executable);
  //Whether it runs a SecondShell.Mcp.exe, wherever it is.
  bool RunsAnyMcpServer(std::string_view entry);
  //A JSON string of the text.
  std::string JsonString(std::string_view text);

  inline const char* const _entryName = "secondshell";
}
