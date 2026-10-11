#pragma once
#include "ActionResult.h"
#include "AppEnvironment.h"
#include "CaptureActions.h"
#include "HostProtocol.h"

namespace SecondShell::Agent
{
  //Where a capture goes.
  struct CaptureDestination
  {
    bool IsInline = true;
    bool IsFile = false;
    bool IsOverInlineLimit = false;  //asked for inline, but the sequence has returned its most inline captures
  };

  //The capture settings, and what happens to captures (Design.md, Capture output): the inline limit per sequence,
  //defaults for the actions, and saving files in the session's capture folder.
  class CaptureStore
  {
  public:
    explicit CaptureStore(Axodox::Infrastructure::dependency_container* container);

    Protocol::CaptureSettings Settings() const;
    void Configure(const Protocol::CaptureSettings& settings);

    CaptureDestination Decide(const Actions::CaptureAction& action, uint32_t inlineCountSoFar) const;

    //Fills in what the action leaves to the settings; inline captures are always PNG.
    void ApplyDefaults(Actions::CaptureAction& action, const CaptureDestination& destination) const;

    //Saves the image if it goes to file, and drops its data unless it goes inline.
    void Store(Actions::CaptureResult& result, const Actions::CaptureAction& action, const CaptureDestination& destination, std::string_view fileStem) const;

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
    std::string _sessionName;

    mutable std::mutex _mutex;
    Protocol::CaptureSettings _settings;
  };
}
