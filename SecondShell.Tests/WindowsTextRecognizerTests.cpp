#include "pch.h"
#include "PhraseMatcher.h"
#include "WindowsTextRecognizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(WindowsTextRecognizerTests)
  {
    //Black text on white, drawn with GDI
    static Pixels Render(const wchar_t* text, uint32_t width, uint32_t height)
    {
      BITMAPINFO info{ .bmiHeader = { .biSize = sizeof(BITMAPINFOHEADER), .biWidth = LONG(width), .biHeight = -LONG(height), .biPlanes = 1, .biBitCount = 32, .biCompression = BI_RGB } };
      void* bits = nullptr;
      auto context = CreateCompatibleDC(nullptr);
      auto bitmap = CreateDIBSection(context, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
      auto font = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
      SelectObject(context, bitmap);
      SelectObject(context, font);

      RECT bounds{ 0, 0, LONG(width), LONG(height) };
      FillRect(context, &bounds, HBRUSH(GetStockObject(WHITE_BRUSH)));
      TextOutW(context, 10, 10, text, int(wcslen(text)));
      GdiFlush();

      Pixels pixels{ width, height, vector<uint8_t>(static_cast<uint8_t*>(bits), static_cast<uint8_t*>(bits) + size_t(width) * height * 4) };
      for (size_t index = 3; index < pixels.Bgra.size(); index += 4) pixels.Bgra[index] = 255;

      DeleteObject(font);
      DeleteObject(bitmap);
      DeleteDC(context);
      return pixels;
    }

    //On a thread in the multithreaded apartment, as in the driver: the engine is awaited by blocking
    static vector<ScreenWord> Recognize(double scale, const Pixels& pixels, const Rect& region)
    {
      vector<ScreenWord> words;
      exception_ptr failure;
      thread{ [&] {
        winrt::init_apartment();
        try
        {
          words = WindowsTextRecognizer{ scale }.Recognize(pixels, region);
        }
        catch (...)
        {
          failure = current_exception();
        }
        winrt::uninit_apartment();
        } }.join();

      if (failure) rethrow_exception(failure);
      return words;
    }

    static void AssertReadsOnTheScreen(double scale)
    {
      Rect region{ 100, 200, 400, 44 };
      auto words = Recognize(scale, Render(L"Typed into the second session", region.Width, region.Height), region);

      Assert::IsTrue(PhraseMatcher{ "Typed into the second session" }.IsIn(words));
      for (auto& word : words) Assert::IsTrue(word.Box.Intersection(region) == word.Box, L"boxes on the screen, inside the region");
      Assert::IsTrue(words.front().Box.X < 100 + 20 && words.front().Box.Y < 200 + 20, L"where it was drawn");
    }

  public:
    TEST_METHOD(TextIsReadWithBoxesOnTheScreen)
    {
      AssertReadsOnTheScreen(1.0);
    }

    TEST_METHOD(UpscaledTextHasBoxesOnTheScreen)
    {
      AssertReadsOnTheScreen(1.5);
    }
  };
}
