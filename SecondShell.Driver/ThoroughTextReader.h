#pragma once
#include "ITextRecognizer.h"
#include "ScreenTextCache.h"

namespace SecondShell::Capture
{
  //The thorough tier of the text waits: the whole screen, in overlapping bands, each read by the thorough recognizer
  //and cached with the fingerprint of its pixels, so a pass over a screen which changed in one place reads only the
  //bands there.
  class ThoroughTextReader
  {
  public:
    explicit ThoroughTextReader(std::shared_ptr<ITextRecognizer> recognizer);

    //The words on the screen, those read twice where bands overlap merged. On a stop request it returns what it read.
    std::vector<ScreenWord> Read(const Pixels& screen, std::stop_token stop);

  private:
    std::shared_ptr<ITextRecognizer> _recognizer;
    ScreenTextCache _bandTexts;
  };
}
