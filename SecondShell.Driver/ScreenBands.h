#pragma once
#include "ScreenWord.h"

namespace SecondShell::Capture
{
  //The thorough tier's split of the screen into horizontal bands: small enough to upscale within an OCR engine's
  //maximum size, and overlapping, so that every line of text is whole in at least one band.
  class ScreenBands
  {
  public:
    static std::vector<Rect> Layout(const Rect& screen);

    //Words read twice where bands overlap, once: of two words at the same place, the larger box is kept, as the edge
    //of a band cuts the other.
    static std::vector<ScreenWord> Merge(std::vector<ScreenWord> words);
  };
}
