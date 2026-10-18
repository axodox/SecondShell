#include "pch.h"
#include "TextLines.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(TextLinesTests)
  {
    static ScreenWord Word(string text, int32_t x, int32_t y)
    {
      return { move(text), { x, y, uint32_t(12 * text.size()), 16 } };
    }

  public:
    TEST_METHOD(WordsAreOrderedIntoLinesByTheirBoxes)
    {
      //As Windows OCR returned a console's banner: words of two lines interleaved
      vector<ScreenWord> words{
        Word("Corporation.", 160, 8), Word("All", 320, 9), Word("rights", 370, 8), Word("C:\\Users\\Axodox>", 0, 40),
        Word("Microsoft", 0, 7), Word("reserved.", 450, 8)
      };

      auto lines = TextLines::FromWords(words);
      Assert::AreEqual(size_t(2), lines.size());
      Assert::AreEqual(string("Microsoft Corporation. All rights reserved."), lines[0]);
      Assert::AreEqual(string("C:\\Users\\Axodox>"), lines[1]);
    }

    TEST_METHOD(WordsOffCentreByLessThanHalfAHeightShareALine)
    {
      vector<ScreenWord> words{ Word("one", 0, 100), Word("two", 50, 103), Word("three", 100, 97), Word("below", 0, 118) };

      auto lines = TextLines::FromWords(words);
      Assert::AreEqual(size_t(2), lines.size());
      Assert::AreEqual(string("one two three"), lines[0]);
      Assert::AreEqual(string("below"), lines[1]);
    }

    TEST_METHOD(NoWordsAreNoLines)
    {
      Assert::IsTrue(TextLines::FromWords({}).empty());
    }
  };
}
