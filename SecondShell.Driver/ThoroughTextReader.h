#pragma once
#include "AreaTextReader.h"

namespace SecondShell::Capture
{
  //The thorough tier of the text waits: the whole screen in overlapping bands (ScreenBands), read by the thorough
  //recognizer. A band is read again only where it changed, so a pass over a screen which changed in one line reads
  //that line only.
  class ThoroughTextReader
  {
  public:
    explicit ThoroughTextReader(std::shared_ptr<ITextRecognizer> recognizer);

    //The words on the screen, those read twice where bands overlap merged. On a stop request it returns what it knew.
    std::vector<ScreenWord> Read(const std::shared_ptr<const ScreenFrame>& frame, const ScreenChanges& changes, std::stop_token stop);

  private:
    AreaTextReader _bandTexts;
  };
}
