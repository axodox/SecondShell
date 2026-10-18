#pragma once
#include "ITextRecognizer.h"

namespace SecondShell::Capture
{
  //Windows.Media.Ocr, in the languages of the user's profile (English if none of them can be recognized). It scales the
  //image first, with high-quality bicubic interpolation, if the engine's maximum size allows: a modest upscale (1.5x,
  //as PowerToys' Text Extractor does) helps with small text.
  class WindowsTextRecognizer : public ITextRecognizer
  {
  public:
    explicit WindowsTextRecognizer(double scale = 1.0);

    //Throws if no OCR language is installed.
    std::vector<ScreenWord> Recognize(const Pixels& pixels, const Rect& region) const override;

  private:
    double _scale;
    winrt::com_ptr<IWICImagingFactory> _factory;
    winrt::Windows::Media::Ocr::OcrEngine _engine{ nullptr };

    double ScaleFor(const Pixels& pixels) const;
    winrt::Windows::Graphics::Imaging::SoftwareBitmap ToBitmap(const Pixels& pixels, double scale) const;
  };
}
