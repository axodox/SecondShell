#include "pch.h"
#include "TextRecognizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(TextRecognizerTests)
  {
  public:
    TEST_METHOD(MatchingForgivesWhatOcrGetsWrong)
    {
      Assert::IsTrue(TextRecognizer::Contains("C:\\>echo SecondShe11", "SecondShell"), L"OCR reads ll as 11 in console fonts");
      Assert::IsTrue(TextRecognizer::Contains("Typed  into\nthe second session", "typed into the SECOND session"), L"case and spacing do not matter");
      Assert::IsTrue(TextRecognizer::Contains("Error C0DE 42", "code 42"), L"O and 0 are alike");
      Assert::IsTrue(TextRecognizer::Contains("Árvíztűrő", "árvíztűrő"));
    }

    TEST_METHOD(OtherTextDoesNotMatch)
    {
      Assert::IsFalse(TextRecognizer::Contains("Save changes?", "Saved"));
      Assert::IsFalse(TextRecognizer::Contains("", "anything"));
    }
  };
}
