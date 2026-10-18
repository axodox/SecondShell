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
  public:
    TEST_METHOD(AreasKeepWhatTheyRead)
    {
      ScreenTextCache cache;
      Rect area{ 10, 10, 100, 50 };
      Assert::IsFalse(cache.Find(area).has_value());

      cache.Store(area, { 1, { { "read", area } } });
      auto known = cache.Find(area);
      Assert::IsTrue(known.has_value());
      Assert::AreEqual(uint64_t(1), known->Fingerprint);
      Assert::AreEqual(string("read"), known->Words[0].Text);

      cache.Store(area, { 2, {} });
      Assert::AreEqual(uint64_t(2), cache.Find(area)->Fingerprint, L"replaced");
      Assert::IsFalse(cache.Find({ 10, 10, 100, 51 }).has_value(), L"another area");
    }

    TEST_METHOD(TheLeastRecentlyUsedAreaIsDropped)
    {
      ScreenTextCache cache{ 2 };
      Rect first{ 0, 0, 10, 10 }, second{ 0, 10, 10, 10 }, third{ 0, 20, 10, 10 };

      cache.Store(first, {});
      cache.Store(second, {});
      cache.Find(first);
      cache.Store(third, {});

      Assert::IsTrue(cache.Find(first).has_value(), L"used recently, so kept");
      Assert::IsFalse(cache.Find(second).has_value(), L"dropped for the third");
      Assert::IsTrue(cache.Find(third).has_value());
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
