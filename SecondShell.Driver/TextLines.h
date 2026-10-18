#pragma once
#include "ScreenWord.h"

namespace SecondShell::Capture
{
  //Lines of text rebuilt from the words' boxes, as OCR engines do not keep a reliable reading order: words whose
  //vertical centres are within half a word's height of the line's first word form a line, left to right; lines run
  //top to bottom.
  class TextLines
  {
  public:
    static std::vector<std::string> FromWords(std::span<const ScreenWord> words);
  };
}
