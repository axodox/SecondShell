#pragma once
#include "AppEnvironment.h"

namespace SecondShell::Setup
{
  //What the elevated setup is to do: enable child sessions, and turn the smooth session view on or off if given.
  struct SetupRequest
  {
    bool EnableChildSessions = false;
    std::optional<bool> SmoothSessionView;

    std::wstring ToArguments() const;
    static std::optional<SetupRequest> FromArguments(std::span<const std::wstring> arguments);  //empty if not --setup
  };

  enum class SetupOutcome
  {
    Done,
    Declined,  //the user said no to the UAC prompt
    Failed
  };

  //Starts SecondShell.exe --setup elevated (UAC) and waits for it. Blocks: not on the UI thread.
  class ElevatedSetupLauncher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ElevatedSetupLauncher" };

  public:
    explicit ElevatedSetupLauncher(Axodox::Infrastructure::dependency_container* container);

    SetupOutcome Run(HWND owner, const SetupRequest& request) const;

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
  };
}
