#include "pch.h"
#include "CaptureStore.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Environment;
using namespace SecondShell::Protocol;
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
    _sessionName(format("{:%Y%m%d-%H%M%S}", floor<seconds>(system_clock::now())))
  { }

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

  CaptureDestination CaptureStore::Decide(const CaptureAction& action, uint32_t inlineCountSoFar) const
  {
    auto output = action.Output.value_or(CaptureOutput::Inline);
    CaptureDestination result{ .IsInline = output != CaptureOutput::File, .IsFile = output != CaptureOutput::Inline };

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

  void CaptureStore::Store(CaptureResult& result, const CaptureAction& action, const CaptureDestination& destination, string_view fileStem) const
  {
    if (destination.IsFile)
    {
      auto extension = result.Image.Format == ImageFormat::Jpeg ? ".jpg" : ".png";
      auto file = action.Path ? path(winrt::to_hstring(*action.Path).c_str()) : _environment->CaptureFolder(_sessionName) / (SafeFileName(fileStem) + extension);
      create_directories(file.parent_path());

      ofstream stream(file, ios::binary);
      stream.write(reinterpret_cast<const char*>(result.Image.Data.data()), streamsize(result.Image.Data.size()));
      if (!stream) throw runtime_error(format("The capture could not be saved to {}.", winrt::to_string(file.wstring())));

      result.Image.Path = winrt::to_string(file.wstring());
    }

    if (!destination.IsInline) result.Image.Data.clear();
    if (destination.IsOverInlineLimit) result.Message = format("Saved to file instead of inline: the sequence has returned its most inline captures ({}).", Settings().InlineLimit);
  }
}
