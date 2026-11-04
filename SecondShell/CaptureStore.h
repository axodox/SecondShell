#pragma once
#include "ActionResult.h"
#include "AppEnvironment.h"
#include "CaptureActions.h"
#include "ClipboardActions.h"
#include "HostProtocol.h"
#include "SettingsStore.h"

namespace SecondShell::Agent
{
  //Where a capture goes.
  struct CaptureDestination
  {
    bool IsInline = true;
    bool IsFile = false;
    bool IsOverInlineLimit = false;  //asked for inline, but the sequence has returned its most inline captures
  };

  //The capture settings, and what happens to captures and clipboard images (Design.md, Capture output): the inline
  //limit per sequence, defaults for the actions, and saving files in the session's capture folder. The format and the
  //inline limit start as the user's settings, and follow them; capture/configure changes them for this run.
  //A file of the captures or the output of processes, for the storage limit.
  struct StoredFile
  {
    std::filesystem::path Path;
    uint64_t Size = 0;
    std::filesystem::file_time_type Written;
  };

  class CaptureStore
  {
    inline static const Axodox::Infrastructure::logger _logger{ "CaptureStore" };

  public:
    explicit CaptureStore(Axodox::Infrastructure::dependency_container* container);
    ~CaptureStore();

    Protocol::CaptureSettings Settings() const;
    void Configure(const Protocol::CaptureSettings& settings);

    //Deletes the capture folders of earlier runs older than the settings keep them, then the oldest captures and output
    //files beyond the storage limit (Design.md, Files).
    void RemoveOldCaptures() const;

    //Does that now, in the background, and then every few minutes, until destroyed.
    void StartCleanup();

    //The oldest files to delete, so the rest take at most the limit in bytes.
    static std::vector<std::filesystem::path> OverLimit(std::vector<StoredFile> files, uint64_t limit);

    CaptureDestination Decide(const std::optional<Actions::CaptureOutput>& output, uint32_t inlineCountSoFar) const;

    //Fills in what the action leaves to the settings; inline captures are always PNG.
    void ApplyDefaults(Actions::CaptureAction& action, const CaptureDestination& destination) const;
    void ApplyDefaults(Actions::ClipboardReadAction& action) const;

    //Saves the image if it goes to file (to the path, if given), and drops its data unless it goes inline. Notes on the
    //result when the inline limit sent the image to file. Returns the file the image is in; an inline one is saved too,
    //in the capture folder's "inline" folder, so the action log can show it.
    std::filesystem::path Store(Actions::CapturedImage& image, Actions::ActionResult& result, const std::optional<std::string>& path, const CaptureDestination& destination, std::string_view fileStem) const;

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
    std::shared_ptr<Session::SettingsStore> _settingsStore;
    std::string _sessionName;

    mutable std::mutex _mutex;
    Protocol::CaptureSettings _settings;

    Axodox::Infrastructure::event_subscription _settingsChangedSubscription;
    void OnSettingsChanged(Session::SettingsStore* sender, const Session::Settings& settings);

    std::jthread _cleanup;
    void LimitStorage() const;
  };
}
