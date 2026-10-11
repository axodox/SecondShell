#pragma once
#include "AppEnvironment.h"

namespace SecondShell::Mcp
{
  //Starts the host, SecondShell.exe, through the running Explorer, so that it runs outside the AI client's package
  //context and process tree. A packaged client's registry virtualization would hide the host's RunOnce entry from
  //Explorer (Findings.md 2.12), and clients end the process trees of their MCP servers, which the host must outlive.
  class HostLauncher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "HostLauncher" };

  public:
    explicit HostLauncher(Axodox::Infrastructure::dependency_container* container);

    //Returns once Explorer has started it; throws if it cannot.
    void Launch();

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
  };
}
