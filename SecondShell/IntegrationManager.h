#pragma once
#include "AiClientIntegration.h"
#include "AppEnvironment.h"

namespace SecondShell::Integrations
{
  //The AI apps SecondShell can add itself to; used by the Add to AI apps page, --integrate and --unintegrate.
  class IntegrationManager
  {
    inline static const Axodox::Infrastructure::logger _logger{ "IntegrationManager" };

  public:
    explicit IntegrationManager(Axodox::Infrastructure::dependency_container* container);
    IntegrationManager(std::vector<std::shared_ptr<AiClientIntegration>> integrations, McpLaunch launch);  //tests

    //All of them, as configured for the folders and the MCP server given; tests use their own folders.
    static std::vector<std::shared_ptr<AiClientIntegration>> Create(const UserFolders& folders, const McpLaunch& launch);

    const std::vector<std::shared_ptr<AiClientIntegration>>& Integrations() const;
    std::shared_ptr<AiClientIntegration> Find(std::string_view id) const;

    //For an app SecondShell does not know: the entry to paste into its MCP configuration.
    std::string Snippet() const;

  private:
    McpLaunch _launch;
    std::vector<std::shared_ptr<AiClientIntegration>> _integrations;
  };
}
