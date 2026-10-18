#include "pch.h"
#include "TextRecognizer.h"

using namespace std;
using namespace winrt::Windows::Globalization;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Storage::Streams;

namespace SecondShell::Capture
{
  namespace
  {
    //Lowercase, without spacing, look-alikes as one character
    wstring Normalize(string_view text)
    {
      //All of Unicode, unlike towlower in the C locale
      wstring lowercase{ winrt::to_hstring(text) };
      CharLowerBuffW(lowercase.data(), DWORD(lowercase.size()));

      wstring result;
      for (auto character : lowercase)
      {
        if (iswspace(character)) continue;

        switch (character)
        {
        case L'i':
        case L'1':
        case L'|':
          character = L'l';
          break;
        case L'0':
          character = L'o';
          break;
        }
        result += character;
      }
      return result;
    }
  }

  TextRecognizer::TextRecognizer() :
    _engine(OcrEngine::TryCreateFromUserProfileLanguages())
  {
    if (!_engine) _engine = OcrEngine::TryCreateFromLanguage(Language{ L"en-US" });
  }

  string TextRecognizer::Recognize(const Pixels& pixels) const
  {
    if (!_engine) throw runtime_error("No OCR language is installed; add one under Settings, Time & language, Language.");
    if (max(pixels.Width, pixels.Height) > OcrEngine::MaxImageDimension()) throw invalid_argument(format("Text can be recognized in images of at most {} pixels a side.", OcrEngine::MaxImageDimension()));

    Buffer buffer{ uint32_t(pixels.Bgra.size()) };
    memcpy(buffer.data(), pixels.Bgra.data(), pixels.Bgra.size());
    buffer.Length(uint32_t(pixels.Bgra.size()));

    auto bitmap = SoftwareBitmap::CreateCopyFromBuffer(buffer, BitmapPixelFormat::Bgra8, int32_t(pixels.Width), int32_t(pixels.Height), BitmapAlphaMode::Premultiplied);
    auto result = _engine.RecognizeAsync(bitmap).get();

    string text;
    for (auto line : result.Lines()) text += winrt::to_string(line.Text()) + "\n";
    return text;
  }

  bool TextRecognizer::Contains(string_view recognized, string_view text)
  {
    return Normalize(recognized).find(Normalize(text)) != wstring::npos;
  }
}
