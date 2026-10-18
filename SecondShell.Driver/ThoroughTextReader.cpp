#include "pch.h"
#include "ThoroughTextReader.h"
#include "ScreenBands.h"

using namespace std;

namespace SecondShell::Capture
{
  ThoroughTextReader::ThoroughTextReader(shared_ptr<ITextRecognizer> recognizer) :
    _bandTexts(move(recognizer))
  { }

  vector<ScreenWord> ThoroughTextReader::Read(const shared_ptr<const ScreenFrame>& frame, const ScreenChanges& changes, stop_token stop)
  {
    vector<ScreenWord> words;
    for (auto& bandWords : _bandTexts.Read(frame, ScreenBands::Layout(frame->Bounds()), changes, stop))
    {
      ranges::move(bandWords, back_inserter(words));
    }
    return ScreenBands::Merge(move(words));
  }
}
