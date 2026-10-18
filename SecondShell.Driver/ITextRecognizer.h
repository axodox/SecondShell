#pragma once
#include "Pixels.h"
#include "ScreenWord.h"

namespace SecondShell::Capture
{
  //An OCR engine, or a pipeline of them (e.g. a text detector feeding another recognizer). The text waits use one per
  //tier, so engines can be swapped or mixed without changing the waits.
  class ITextRecognizer
  {
  public:
    virtual ~ITextRecognizer() = default;

    //The words in the pixels, which show the region of the screen; their boxes are on the screen.
    virtual std::vector<ScreenWord> Recognize(const Pixels& pixels, const Rect& region) const = 0;
  };
}
