#include "pch.h"
#include "Pixels.h"
#include "ScreenTextCache.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ScreenTextCacheTests)
  {
    int _reads = 0;

    vector<ScreenWord> Read(ScreenTextCache& cache, const Rect& area, uint64_t fingerprint)
    {
      return cache.Read(area, fingerprint, [&] {
        _reads++;
        return vector<ScreenWord>{ { format("read {}", _reads), area } };
        });
    }

  public:
    TEST_METHOD(AnAreaIsReadAgainOnlyOnceItsPixelsChange)
    {
      ScreenTextCache cache;
      Rect area{ 10, 10, 100, 50 };

      Assert::AreEqual(string("read 1"), Read(cache, area, 1)[0].Text);
      Assert::AreEqual(string("read 1"), Read(cache, area, 1)[0].Text, L"from the cache");
      Assert::AreEqual(string("read 2"), Read(cache, area, 2)[0].Text, L"other pixels");
      Assert::AreEqual(string("read 3"), Read(cache, { 10, 10, 100, 51 }, 2)[0].Text, L"another area");
      Assert::AreEqual(3, _reads);
    }

    TEST_METHOD(TheLeastRecentlyUsedAreaIsDropped)
    {
      ScreenTextCache cache{ 2 };
      Rect first{ 0, 0, 10, 10 }, second{ 0, 10, 10, 10 }, third{ 0, 20, 10, 10 };

      Read(cache, first, 1);
      Read(cache, second, 1);
      Read(cache, first, 1);
      Read(cache, third, 1);
      Assert::AreEqual(3, _reads);

      Read(cache, first, 1);
      Assert::AreEqual(3, _reads, L"used recently, so kept");
      Read(cache, second, 1);
      Assert::AreEqual(4, _reads, L"dropped for the third");
    }

    TEST_METHOD(FingerprintsTellChangedPixels)
    {
      Pixels pixels{ 64, 32, vector<uint8_t>(64 * 32 * 4, 200) };
      auto whole = pixels.Fingerprint();
      auto left = pixels.Fingerprint({ 0, 0, 32, 32 });

      pixels.Bgra[(10 * 64 + 40) * 4 + 1] = 0;
      Assert::AreNotEqual(whole, pixels.Fingerprint());
      Assert::AreEqual(left, pixels.Fingerprint({ 0, 0, 32, 32 }), L"the change is outside");
      Assert::AreNotEqual(pixels.Fingerprint({ 32, 0, 32, 32 }), pixels.Fingerprint({ 0, 0, 32, 32 }));
    }

    TEST_METHOD(CropsCopyTheArea)
    {
      Pixels pixels{ 4, 3, vector<uint8_t>(4 * 3 * 4) };
      for (size_t index = 0; index < pixels.Bgra.size(); index++) pixels.Bgra[index] = uint8_t(index);

      auto crop = pixels.Crop({ 1, 1, 2, 2 });
      Assert::AreEqual(2u, crop.Width);
      Assert::AreEqual(2u, crop.Height);
      Assert::AreEqual(uint8_t((1 * 4 + 1) * 4), crop.Bgra[0]);
      Assert::AreEqual(uint8_t((2 * 4 + 2) * 4 + 3), crop.Bgra.back());
    }
  };
}
