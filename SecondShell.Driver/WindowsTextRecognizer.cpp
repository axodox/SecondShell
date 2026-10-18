#include "pch.h"
#include "WindowsTextRecognizer.h"

using namespace std;
using namespace winrt::Windows::Globalization;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Storage::Streams;

namespace SecondShell::Capture
{
  namespace
  {
    //Outwards to whole screen pixels
    Rect ToScreen(const winrt::Windows::Foundation::Rect& box, const Rect& region, double scale)
    {
      auto left = int32_t(floor(box.X / scale));
      auto top = int32_t(floor(box.Y / scale));
      auto right = int32_t(ceil((box.X + box.Width) / scale));
      auto bottom = int32_t(ceil((box.Y + box.Height) / scale));
      return { region.X + left, region.Y + top, uint32_t(right - left), uint32_t(bottom - top) };
    }
  }

  WindowsTextRecognizer::WindowsTextRecognizer(double scale) :
    _scale(scale),
    _factory(winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory)),
    _engine(OcrEngine::TryCreateFromUserProfileLanguages())
  {
    if (!_engine) _engine = OcrEngine::TryCreateFromLanguage(Language{ L"en-US" });
  }

  vector<ScreenWord> WindowsTextRecognizer::Recognize(const Pixels& pixels, const Rect& region) const
  {
    if (!_engine) throw runtime_error("No OCR language is installed; add one under Settings, Time & language, Language.");

    auto scale = ScaleFor(pixels);
    auto result = _engine.RecognizeAsync(ToBitmap(pixels, scale)).get();

    vector<ScreenWord> words;
    for (auto line : result.Lines())
    {
      for (auto word : line.Words()) words.push_back({ winrt::to_string(word.Text()), ToScreen(word.BoundingRect(), region, scale) });
    }
    return words;
  }

  double WindowsTextRecognizer::ScaleFor(const Pixels& pixels) const
  {
    auto side = double(max(pixels.Width, pixels.Height));
    auto maxSide = double(OcrEngine::MaxImageDimension());
    if (side * _scale <= maxSide) return _scale;
    if (side <= maxSide) return 1.0;

    throw invalid_argument(format("Text can be recognized in images of at most {} pixels a side.", OcrEngine::MaxImageDimension()));
  }

  SoftwareBitmap WindowsTextRecognizer::ToBitmap(const Pixels& pixels, double scale) const
  {
    auto width = max(1u, uint32_t(lround(pixels.Width * scale)));
    auto height = max(1u, uint32_t(lround(pixels.Height * scale)));
    auto size = width * height * 4;

    Buffer buffer{ size };
    buffer.Length(size);
    if (scale == 1.0)
    {
      memcpy(buffer.data(), pixels.Bgra.data(), size);
    }
    else
    {
      winrt::com_ptr<IWICBitmap> bitmap;
      winrt::check_hresult(_factory->CreateBitmapFromMemory(pixels.Width, pixels.Height, GUID_WICPixelFormat32bppBGRA, pixels.Width * 4, UINT(pixels.Bgra.size()), const_cast<BYTE*>(pixels.Bgra.data()), bitmap.put()));

      winrt::com_ptr<IWICBitmapScaler> scaler;
      winrt::check_hresult(_factory->CreateBitmapScaler(scaler.put()));
      winrt::check_hresult(scaler->Initialize(bitmap.get(), width, height, WICBitmapInterpolationModeHighQualityCubic));
      winrt::check_hresult(scaler->CopyPixels(nullptr, width * 4, size, buffer.data()));
    }

    return SoftwareBitmap::CreateCopyFromBuffer(buffer, BitmapPixelFormat::Bgra8, int32_t(width), int32_t(height), BitmapAlphaMode::Premultiplied);
  }
}
