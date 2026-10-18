#pragma once
#include "ScreenCapturer.h"

namespace SecondShell::Capture
{
  //Windows.Media.Ocr, in the languages of the user's profile (English if none of them can be recognized).
  class TextRecognizer
  {
  public:
    TextRecognizer();

    //The text of the pixels, its lines separated by line breaks; throws if no OCR language is installed.
    std::string Recognize(const Pixels& pixels) const;

    //Whether the text contains what is looked for, as far as OCR can tell: case and spacing do not matter, and
    //characters it confuses (l, I, 1 and |; O and 0) count as the same.
    static bool Contains(std::string_view recognized, std::string_view text);

  private:
    winrt::Windows::Media::Ocr::OcrEngine _engine{ nullptr };
  };
}
