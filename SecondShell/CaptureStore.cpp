#include "pch.h"
#include "CaptureStore.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Environment;
using namespace SecondShell::Protocol;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;
using namespace std::filesystem;

namespace SecondShell::Agent
{
  namespace
  {
    //A file name part from text the AI chose
    string SafeFileName(string_view text)
    {
      string result{ text };
      for (auto& character : result)
      {
        if (!isalnum(uint8_t(character)) && character != '-' && character != '_' && character != '.') character = '_';
      }
      return result;
    }
  }

  CaptureStore::CaptureStore(dependency_container* container) :
    _environment(container->resolve<AppEnvironment>()),
    _settingsStore(container->resolve<SettingsStore>()),
    _sessionName(format("{:%Y%m%d-%H%M%S}", floor<seconds>(system_clock::now()))),
    _settingsChangedSubscription(_settingsStore->Changed({ this, &CaptureStore::OnSettingsChanged }))
  {
    OnSettingsChanged(_settingsStore.get(), _settingsStore->Current());
  }

  CaptureSettings CaptureStore::Settings() const
  {
    lock_guard lock{ _mutex };
    return _settings;
  }

  void CaptureStore::Configure(const CaptureSettings& settings)
  {
    lock_guard lock{ _mutex };
    _settings = settings;
  }

  void CaptureStore::RemoveOldCaptures() const
  {
    auto retention = _settingsStore->Current().CaptureRetention;
    auto captures = _environment->CapturesFolder();
    if (retention == 0 || !exists(captures)) return;

    //By the time of their last write, which is the run's last capture
    auto oldest = file_time_type::clock::now() - days(retention);
    error_code error;
    for (auto& folder : directory_iterator(captures, error))
    {
      if (!folder.is_directory() || folder.path().filename() == _sessionName || folder.last_write_time() >= oldest) continue;
      remove_all(folder.path(), error);
    }
  }

  void CaptureStore::OnSettingsChanged(SettingsStore* /*sender*/, const Session::Settings& settings)
  {
    lock_guard lock{ _mutex };
    _settings.Format = settings.CaptureFormat;
    _settings.InlineLimit = settings.InlineCaptureLimit;
  }

  CaptureDestination CaptureStore::Decide(const optional<CaptureOutput>& output, uint32_t inlineCountSoFar) const
  {
    auto target = output.value_or(CaptureOutput::Inline);
    CaptureDestination result{ .IsInline = target != CaptureOutput::File, .IsFile = target != CaptureOutput::Inline };

    //Beyond the limit, inline captures go to file, so a runaway sequence cannot flood the AI's context
    if (result.IsInline && inlineCountSoFar >= Settings().InlineLimit)
    {
      result = { .IsInline = false, .IsFile = true, .IsOverInlineLimit = true };
    }
    return result;
  }

  void CaptureStore::ApplyDefaults(CaptureAction& action, const CaptureDestination& destination) const
  {
    auto settings = Settings();
    if (!action.Resolution) action.Resolution = settings.Resolution;
    action.Format = destination.IsInline ? ImageFormat::Png : action.Format.value_or(settings.Format);
  }

  void CaptureStore::ApplyDefaults(ClipboardReadAction& action) const
  {
    if (!action.Resolution) action.Resolution = Settings().Resolution;
  }

  path CaptureStore::Store(CapturedImage& image, ActionResult& result, const optional<string>& filePath, const CaptureDestination& destination, string_view fileStem) const
  {
    auto name = SafeFileName(fileStem) + (image.Format == ImageFormat::Jpeg ? ".jpg" : ".png");
    auto folder = _environment->CaptureFolder(_sessionName);
    auto file = !destination.IsFile ? folder / "inline" / name : filePath ? path(winrt::to_hstring(*filePath).c_str()) : folder / name;
    create_directories(file.parent_path());

    ofstream stream(file, ios::binary);
    stream.write(reinterpret_cast<const char*>(image.Data.data()), streamsize(image.Data.size()));
    if (!stream) throw runtime_error(format("The capture could not be saved to {}.", winrt::to_string(file.wstring())));

    if (destination.IsFile) image.Path = winrt::to_string(file.wstring());
    if (!destination.IsInline) image.Data.clear();
    if (destination.IsOverInlineLimit) result.Message = format("Saved to file instead of inline: the sequence has returned its most inline captures ({}).", Settings().InlineLimit);
    return file;
  }
}
