#include "pch.h"
#include "QuickTextReader.h"

using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Capture
{
  QuickTextReader::QuickTextReader(shared_ptr<IWindowTextSource> exposedText, shared_ptr<ITextRecognizer> recognizer) :
    _exposedText(move(exposedText)),
    _recognizer(move(recognizer))
  { }

  WindowText QuickTextReader::Read(const Pixels& screen, WindowId window, const Rect& area)
  {
    auto fingerprint = screen.Fingerprint(area);
    return {
      .Exposed = _exposedTexts.Read(area, fingerprint, [&] { return _exposedText->Read(window); }),
      .Recognized = _recognizedTexts.Read(area, fingerprint, [&] { return _recognizer->Recognize(screen.Crop(area), area); })
    };
  }
}
