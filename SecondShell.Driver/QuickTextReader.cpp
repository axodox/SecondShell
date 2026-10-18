#include "pch.h"
#include "QuickTextReader.h"

using namespace std;

namespace SecondShell::Capture
{
  QuickTextReader::QuickTextReader(shared_ptr<IWindowTextSource> exposedText, shared_ptr<ITextRecognizer> recognizer) :
    _exposedText(move(exposedText)),
    _recognizedTexts(move(recognizer))
  { }

  vector<WindowText> QuickTextReader::Read(const shared_ptr<const ScreenFrame>& frame, span<const WindowArea> windows, const ScreenChanges& changes)
  {
    vector<Rect> areas;
    for (auto& window : windows) areas.push_back(window.Area);
    auto recognized = _recognizedTexts.Read(frame, areas, changes);

    vector<WindowText> texts;
    for (size_t index = 0; index < windows.size(); index++)
    {
      texts.push_back({ ReadExposed(frame->CpuPixels(), windows[index]), move(recognized[index]) });
    }
    return texts;
  }

  vector<ScreenWord> QuickTextReader::ReadExposed(const Pixels& screen, const WindowArea& window)
  {
    auto fingerprint = screen.Fingerprint(window.Area);
    auto known = _exposedTexts.Find(window.Area);
    if (known && known->Fingerprint == fingerprint) return move(known->Words);

    auto words = _exposedText->Read(window.Window);
    _exposedTexts.Store(window.Area, { fingerprint, words });
    return words;
  }
}
