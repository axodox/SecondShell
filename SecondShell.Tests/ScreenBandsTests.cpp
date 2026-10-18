#include "pch.h"
#include "ScreenBands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ScreenBandsTests)
  {
  public:
    TEST_METHOD(BandsCoverTheScreenAndOverlap)
    {
      auto bands = ScreenBands::Layout({ 0, 0, 1920, 1080 });

      Assert::AreEqual(size_t(5), bands.size());
      Assert::AreEqual(0, bands.front().Y);
      Assert::AreEqual(1080, bands.back().Bottom(), L"the last band ends at the bottom");
      for (size_t index = 0; index < bands.size(); index++)
      {
        Assert::IsTrue(bands[index] == Rect{ 0, bands[index].Y, 1920, 300 }, L"full width, all of the same height");
        if (index > 0) Assert::IsTrue(bands[index - 1].Bottom() - bands[index].Y >= 48, L"each overlaps the one above");
      }
    }

    TEST_METHOD(AShortScreenIsOneBand)
    {
      auto bands = ScreenBands::Layout({ 0, 0, 800, 200 });

      Assert::AreEqual(size_t(1), bands.size());
      Assert::IsTrue(bands[0] == Rect{ 0, 0, 800, 200 });
    }

    TEST_METHOD(WordsReadTwiceInAnOverlapAreKeptOnce)
    {
      auto words = ScreenBands::Merge({
        { "reserved", { 100, 290, 80, 10 } },  //cut by the upper band's edge
        { "rights", { 40, 280, 50, 18 } },
        { "reserved.", { 100, 280, 82, 18 } },
        { "rights", { 41, 281, 49, 17 } },
        { "All", { 0, 280, 30, 18 } }
      });

      ranges::sort(words, {}, [](const ScreenWord& word) { return word.Box.X; });
      Assert::AreEqual(size_t(3), words.size());
      Assert::AreEqual(string("All"), words[0].Text);
      Assert::AreEqual(string("rights"), words[1].Text);
      Assert::AreEqual(string("reserved."), words[2].Text, L"the whole word, not the cut one");
    }
  };
}
