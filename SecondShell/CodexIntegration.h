#pragma once
#include "AiClientIntegration.h"

namespace SecondShell::Integrations
{
  //OpenAI Codex (its CLI, IDE extension and the ChatGPT desktop app share one file): [mcp_servers.secondshell] in
  //config.toml, in %CODEX_HOME% or ~/.codex.
  class CodexIntegration : public AiClientIntegration
  {
  public:
    CodexIntegration(const UserFolders& folders, std::filesystem::path mcpExecutable);

    virtual std::string_view Id() const override;
    virtual std::string_view Name() const override;
    virtual std::string Where() const override;

    virtual IntegrationState State() const override;
    virtual IntegrationChange PreviewAdd() const override;
    virtual IntegrationChange PreviewRemove() const override;

    virtual void Add() override;
    virtual void Remove() override;

  private:
    std::filesystem::path _home;
    std::filesystem::path _mcpExecutable;

    std::filesystem::path ConfigFile() const;
  };
}
