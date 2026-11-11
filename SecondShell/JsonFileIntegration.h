#pragma once
#include "AiClientIntegration.h"
#include "JsonConfigEditor.h"
#include "McpLaunch.h"

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
    JsonFileIntegration(JsonFileClient client, McpLaunch launch);

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
    McpLaunch _launch;

    JsonEntry Entry() const;
    bool IsInstalled() const;

    //The name SecondShell is under: "secondshell", or the name of an entry added by hand that runs a
    //SecondShell.Mcp.exe, so it is updated rather than added twice.
    std::optional<std::string> EntryName(const JsonConfigEditor& editor) const;
  };

  //Whether an entry's "command" and "args" start the server as the launch does; JSON text of the entry.
  bool RunsLaunch(std::string_view entry, const McpLaunch& launch);
  //Whether it runs a SecondShell.Mcp.exe, wherever it is, directly or through cmd.
  bool RunsAnyMcpServer(std::string_view entry);
  //A JSON string of the text, and a JSON array of the texts.
  std::string JsonString(std::string_view text);
  std::string JsonArray(const std::vector<std::string>& texts);

  inline const char* const _entryName = "secondshell";
}
