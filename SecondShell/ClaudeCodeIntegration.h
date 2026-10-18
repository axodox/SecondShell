#pragma once
#include "AiClientIntegration.h"
#include "CliRunner.h"

namespace SecondShell::Integrations
{
  //Claude Code, through its own command line (claude mcp add / remove, user scope): it rewrites ~/.claude.json often,
  //so the file is only read, to tell the state.
  class ClaudeCodeIntegration : public AiClientIntegration
  {
  public:
    ClaudeCodeIntegration(const UserFolders& folders, std::filesystem::path mcpExecutable);

    virtual std::string_view Id() const override;
    virtual std::string_view Name() const override;
    virtual std::string Where() const override;

    virtual IntegrationState State() const override;
    virtual IntegrationChange PreviewAdd() const override;
    virtual IntegrationChange PreviewRemove() const override;

    virtual void Add() override;
    virtual void Remove() override;

  private:
    std::filesystem::path _configFile;
    std::filesystem::path _nativeLauncher;
    std::filesystem::path _mcpExecutable;
    CliRunner _cli;

    std::string Tool() const;
    std::vector<std::string> AddArguments() const;
    std::vector<std::string> RemoveArguments() const;
    void Run(std::span<const std::string> arguments) const;
  };
}
