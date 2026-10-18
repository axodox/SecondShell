#include "pch.h"
#include "ThoroughTextReader.h"
#include "ScreenBands.h"

using namespace std;

namespace SecondShell::Capture
{
  ThoroughTextReader::ThoroughTextReader(shared_ptr<ITextRecognizer> recognizer) :
    _recognizer(move(recognizer))
  { }

  vector<ScreenWord> ThoroughTextReader::Read(const Pixels& screen, stop_token stop)
  {
    vector<ScreenWord> words;
    for (auto& band : ScreenBands::Layout(screen.Bounds()))
    {
      if (stop.stop_requested()) break;

      auto bandWords = _bandTexts.Read(band, screen.Fingerprint(band), [&] { return _recognizer->Recognize(screen.Crop(band), band); });
      ranges::move(bandWords, back_inserter(words));
    }
    return ScreenBands::Merge(move(words));
  }
}
