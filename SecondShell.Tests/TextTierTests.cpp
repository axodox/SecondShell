#include "pch.h"
#include "FakeTextRecognizer.h"
#include "QuickTextReader.h"
#include "ThoroughTextReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  namespace
  {
    class FakeWindowTextSource : public IWindowTextSource
    {
    public:
      mutable vector<WindowId> Windows;

      vector<ScreenWord> Read(WindowId window) const override
      {
        Windows.push_back(window);
        return { { "Command Prompt", { 0, 0, 200, 20 } } };
      }
    };

    ScreenChanges Unknown(const ScreenFrame& frame)
    {
      DirtyRegion all{ frame.Bounds() };
      all.AddAll();
      return { move(all), nullptr };
    }
  }

  TEST_CLASS(QuickTextReaderTests)
  {
    shared_ptr<FakeWindowTextSource> _exposedText = make_shared<FakeWindowTextSource>();
    shared_ptr<FakeTextRecognizer> _recognizer = make_shared<FakeTextRecognizer>();
    QuickTextReader _reader{ _exposedText, _recognizer };

  public:
    TEST_METHOD(EachWindowIsReadFromBothSources)
    {
      _recognizer->Words = { { "Microsoft", { 110, 210, 80, 16 } }, { "Explorer", { 510, 20, 80, 16 } }, { "elsewhere", { 700, 500, 80, 16 } } };
      vector<WindowArea> windows{ { { 0x42 }, { 100, 200, 300, 150 } }, { { 0x43 }, { 500, 10, 200, 100 } } };
      auto frame = Frame(1, 800, 600);

      auto texts = _reader.Read(frame, windows, Unknown(*frame));

      Assert::AreEqual(size_t(2), texts.size());
      Assert::AreEqual(string("Command Prompt"), texts[0].Exposed[0].Text);
      Assert::IsTrue(_exposedText->Windows == vector<WindowId>{ { 0x42 }, { 0x43 } });

      Assert::AreEqual(size_t(1), _recognizer->Calls(), L"both windows in one atlas");
      Assert::AreEqual(size_t(1), texts[0].Recognized.size(), L"only the window's area");
      Assert::AreEqual(string("Microsoft"), texts[0].Recognized[0].Text);
      Assert::AreEqual(string("Explorer"), texts[1].Recognized[0].Text);
    }

    TEST_METHOD(AnUnchangedWindowIsNotReadAgain)
    {
      vector<WindowArea> windows{ { { 0x42 }, { 100, 200, 300, 150 } } };
      auto frame = Frame(1, 800, 600);
      _reader.Read(frame, windows, Unknown(*frame));
      _reader.Read(Frame(2, 800, 600), windows, Unknown(*frame));
      Assert::AreEqual(size_t(1), _exposedText->Windows.size());
      Assert::AreEqual(size_t(1), _recognizer->Calls());

      _reader.Read(Changed(*frame, 10, 10), windows, Unknown(*frame));
      Assert::AreEqual(size_t(1), _recognizer->Calls(), L"changed outside the window");

      _reader.Read(Changed(*frame, 150, 250), windows, Unknown(*frame));
      Assert::AreEqual(size_t(2), _exposedText->Windows.size());
      Assert::AreEqual(size_t(2), _recognizer->Calls());
    }
  };

  TEST_CLASS(ThoroughTextReaderTests)
  {
    shared_ptr<FakeTextRecognizer> _recognizer = make_shared<FakeTextRecognizer>();
    ThoroughTextReader _reader{ _recognizer };

  public:
    TEST_METHOD(TheScreenIsReadInBandsAndOverlapsMerged)
    {
      //The first is in the overlap of the first two bands, so both read it
      _recognizer->Words = { { "overlap", { 10, 260, 60, 16 } }, { "top", { 10, 10, 30, 16 } }, { "bottom", { 10, 1060, 50, 16 } } };
      auto frame = Frame(1, 400, 1080);

      auto words = _reader.Read(frame, Unknown(*frame), {});

      Assert::AreEqual(size_t(5), _recognizer->Read().size(), L"every band");
      ranges::sort(words, {}, [](const ScreenWord& word) { return word.Box.Y; });
      Assert::AreEqual(size_t(3), words.size());
      Assert::AreEqual(string("top"), words[0].Text);
      Assert::AreEqual(string("overlap"), words[1].Text, L"once");
      Assert::AreEqual(string("bottom"), words[2].Text);
    }

    TEST_METHOD(OnlyTheChangedLinesOfABandAreReadAgain)
    {
      auto first = Frame(1, 400, 1080);
      _reader.Read(first, Unknown(*first), {});
      _reader.Read(Frame(2, 400, 1080), Unknown(*first), {});
      Assert::AreEqual(size_t(1), _recognizer->Calls(), L"a static screen costs no recognition");

      DirtyRegion changes{ first->Bounds() };
      changes.Add({ 200, 600, 1, 1 });
      _reader.Read(Changed(*first, 200, 600), { changes, first }, {});

      Assert::AreEqual(size_t(2), _recognizer->Calls());
      Assert::IsTrue(_recognizer->Read().back() == Rect{ 0, 552, 400, 80 }, L"one strip of the one band which changed");
    }

    TEST_METHOD(AStoppedPassReadsNoMore)
    {
      stop_source stop;
      stop.request_stop();
      auto frame = Frame(1, 400, 1080);

      Assert::IsTrue(_reader.Read(frame, Unknown(*frame), stop.get_token()).empty());
      Assert::AreEqual(size_t(0), _recognizer->Calls());
    }
  };
}
