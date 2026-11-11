#include "pch.h"
#include "AnsiText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Text;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(AnsiTextTests)
  {
  public:
    TEST_METHOD(EscapeCodesAreStripped)
    {
      Assert::AreEqual(string("plain"), StripEscapes("plain"));
      Assert::AreEqual(string("  ready in 120 ms"), StripEscapes("\x1B[32m  ready\x1B[39m in \x1B[1m120\x1B[22m ms"));
      Assert::AreEqual(string("title"), StripEscapes("\x1B]0;npm run dev\x07title"), L"a window title");
      Assert::AreEqual(string("abc"), StripEscapes("a\x1B[2Kb\x1B[1Gc"), L"cursor and erase codes");
      Assert::AreEqual(string("x"), StripEscapes("x\x1B["), L"an unfinished code at the end");
    }

    TEST_METHOD(ColorsAndStylesFollowTheCodes)
    {
      AnsiParser parser;
      auto spans = parser.Parse("\x1B[1;31mERROR\x1B[0m: \x1B[38;5;208morange\x1B[38;2;1;2;3m rgb\x1B[39m end");

      Assert::AreEqual(size_t(5), spans.size());
      Assert::AreEqual(string("ERROR"), spans[0].Text);
      Assert::IsTrue(spans[0].Style.IsBold && spans[0].Style.Foreground == 0xC50F1Fu, L"bold red, as Campbell has it");
      Assert::IsTrue(spans[1].Style == TextStyle{}, L"reset");
      Assert::IsTrue(spans[2].Style.Foreground == PaletteColor(208));
      Assert::IsTrue(spans[3].Style.Foreground == 0x010203u);
      Assert::IsFalse(spans[4].Style.Foreground.has_value(), L"the default color again");
    }

    TEST_METHOD(TheStyleCarriesOnToTheNextLine)
    {
      AnsiParser parser;
      parser.Parse("\x1B[33mwarning: the next line is yellow too");
      auto spans = parser.Parse("still yellow");
      Assert::IsTrue(spans.at(0).Style.Foreground == 0xC19C00u);

      AnsiParser fromStart{ parser.Style() };
      Assert::IsTrue(fromStart.Parse("from the saved style").at(0).Style.Foreground == 0xC19C00u);
    }

    TEST_METHOD(BrightAndBackgroundColors)
    {
      AnsiParser parser;
      auto spans = parser.Parse("\x1B[92;44mok\x1B[7mswap\x1B[27;49m");
      Assert::IsTrue(spans[0].Style.Foreground == 0x16C60Cu && spans[0].Style.Background == 0x0037DAu);
      Assert::IsTrue(spans[1].Style.IsInverse);
      Assert::IsTrue(parser.Style().Background == nullopt && !parser.Style().IsInverse);
    }

    TEST_METHOD(ThePaletteHasTheCubeAndTheGrays)
    {
      Assert::AreEqual(0x000000u, PaletteColor(16));
      Assert::AreEqual(0xFFFFFFu, PaletteColor(231));
      Assert::AreEqual(0x080808u, PaletteColor(232));
      Assert::AreEqual(0xEEEEEEu, PaletteColor(255));
    }
  };
}
