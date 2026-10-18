#pragma once
#include "ScreenWord.h"

namespace SecondShell::Capture
{
  //Whether a phrase is in what was read from the screen, forgiving what OCR gets wrong: both sides are normalized
  //(NFKC, case, spacing, look-alikes such as 0 / O, 1 / l / I and rn / m), and the phrase may differ from the text in
  //about a fifth of its characters. Words are searched in the lines rebuilt from their boxes, joined, so that a phrase
  //may wrap.
  class PhraseMatcher
  {
  public:
    explicit PhraseMatcher(std::string_view phrase);

    bool IsIn(std::span<const ScreenWord> words) const;
    bool IsIn(std::string_view text) const;

    static std::wstring Normalize(std::string_view text);

    //The fewest edits turning the pattern into some substring of the text (Sellers' algorithm).
    static size_t SubstringDistance(std::wstring_view pattern, std::wstring_view text);

  private:
    std::wstring _phrase;
    size_t _allowedEdits;
  };
}
