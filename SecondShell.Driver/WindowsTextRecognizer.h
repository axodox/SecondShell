#pragma once
#include "ITextRecognizer.h"

namespace SecondShell::Capture
{
  //Windows.Media.Ocr, on the CPU, in the languages of the user's profile (English if none of them can be recognized).
  //It scales the atlas first, with high-quality bicubic interpolation, if the engine's maximum size allows: a modest
  //upscale (1.5x, as PowerToys' Text Extractor does) helps with small text.
  class WindowsTextRecognizer : public ITextRecognizer
  {
  public:
    explicit WindowsTextRecognizer(double scale = 1.0);

    uint32_t MaxAtlasSide() const override;

    //Throws if no OCR language is installed.
    std::future<std::vector<AtlasWord>> RecognizeAsync(std::shared_ptr<const TextAtlas> atlas) const override;

  private:
    double _scale;
    winrt::com_ptr<IWICImagingFactory> _factory;
    winrt::Windows::Media::Ocr::OcrEngine _engine{ nullptr };

    double ScaleFor(const Pixels& pixels) const;
    winrt::Windows::Graphics::Imaging::SoftwareBitmap ToBitmap(const Pixels& pixels, double scale) const;
  };
}
