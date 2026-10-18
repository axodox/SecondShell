#include "pch.h"
#include "PhraseMatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(PhraseMatcherTests)
  {
    static bool IsIn(string_view text, string_view phrase)
    {
      return PhraseMatcher{ phrase }.IsIn(text);
    }

  public:
    TEST_METHOD(MatchingForgivesWhatOcrGetsWrong)
    {
      Assert::IsTrue(IsIn("C:\\>echo SecondShe11", "SecondShell"), L"OCR reads ll as 11 in console fonts");
      Assert::IsTrue(IsIn("Typed  into\nthe second session", "typed into the SECOND session"), L"case and spacing do not matter");
      Assert::IsTrue(IsIn("Error C0DE 42", "code 42"), L"O and 0 are alike");
      Assert::IsTrue(IsIn("Modem settings", "Modern settings"), L"rn and m are alike");
      Assert::IsTrue(IsIn("ＳｅｃｏｎｄＳｈｅｌｌ ready", "SecondShell"), L"compatibility forms are normalized");
      Assert::IsTrue(IsIn("Árvíztűrő", "árvíztűrő"));
    }

    TEST_METHOD(AFifthOfThePhraseMayDiffer)
    {
      Assert::IsTrue(IsIn("Microsoft Corporation. Al rigths reserved.", "All rights reserved"), L"three edits in seventeen letters");
      Assert::IsTrue(IsIn("indows tversion 10", "Windows version 10"), L"the first letter dropped at a window's edge, and a stray one");
      Assert::IsFalse(IsIn("No rights given", "All rights reserved"));
    }

    TEST_METHOD(ShortPhrasesMustMatchExactly)
    {
      Assert::IsFalse(IsIn("Save changes?", "Saved"));
      Assert::IsTrue(IsIn("Saved.", "Saved"));
    }

    TEST_METHOD(OtherTextDoesNotMatch)
    {
      Assert::IsFalse(IsIn("", "anything"));
      Assert::IsFalse(IsIn("Save changes?", "Discard all changes"));
    }

    TEST_METHOD(APhraseMayWrapAcrossLines)
    {
      vector<ScreenWord> words{
        { "fox", { 0, 30, 30, 16 } }, { "jumps", { 40, 30, 50, 16 } },
        { "The", { 0, 0, 30, 16 } }, { "quick", { 40, 0, 50, 16 } }, { "brown", { 100, 0, 50, 16 } }
      };
      Assert::IsTrue(PhraseMatcher{ "brown fox jumps" }.IsIn(words));
      Assert::IsFalse(PhraseMatcher{ "jumps the quick" }.IsIn(words), L"lines are read top to bottom");
    }

    TEST_METHOD(SubstringDistanceCountsTheFewestEdits)
    {
      Assert::AreEqual(size_t(0), PhraseMatcher::SubstringDistance(L"abc", L"xxabcxx"));
      Assert::AreEqual(size_t(1), PhraseMatcher::SubstringDistance(L"abc", L"xxabxx"));
      Assert::AreEqual(size_t(1), PhraseMatcher::SubstringDistance(L"abc", L"xxaxcxx"));
      Assert::AreEqual(size_t(1), PhraseMatcher::SubstringDistance(L"abc", L"abbc"));
      Assert::AreEqual(size_t(3), PhraseMatcher::SubstringDistance(L"abc", L""));
    }
  };
}
