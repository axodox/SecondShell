#include "pch.h"
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
    //Reads the words it is given which are whole in the region
    class FakeRecognizer : public ITextRecognizer
    {
    public:
      vector<ScreenWord> Words;
      mutable vector<Rect> Regions;

      vector<ScreenWord> Recognize(const Pixels& pixels, const Rect& region) const override
      {
        Assert::AreEqual(region.Width, pixels.Width, L"the region's pixels");
        Assert::AreEqual(region.Height, pixels.Height);
        Regions.push_back(region);

        vector<ScreenWord> result;
        for (auto& word : Words)
        {
          if (word.Box.Intersection(region) == word.Box) result.push_back(word);
        }
        return result;
      }
    };

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

    Pixels Screen(uint32_t width, uint32_t height)
    {
      return { width, height, vector<uint8_t>(size_t(width) * height * 4, 255) };
    }

    void Change(Pixels& pixels, uint32_t x, uint32_t y)
    {
      pixels.Bgra[(size_t(y) * pixels.Width + x) * 4] ^= 0xFF;
    }
  }

  TEST_CLASS(QuickTextReaderTests)
  {
    shared_ptr<FakeWindowTextSource> _exposedText = make_shared<FakeWindowTextSource>();
    shared_ptr<FakeRecognizer> _recognizer = make_shared<FakeRecognizer>();
    QuickTextReader _reader{ _exposedText, _recognizer };

  public:
    TEST_METHOD(AWindowIsReadFromBothSources)
    {
      _recognizer->Words = { { "Microsoft", { 110, 210, 80, 16 } }, { "elsewhere", { 600, 10, 80, 16 } } };

      auto text = _reader.Read(Screen(800, 600), { 0x42 }, { 100, 200, 300, 150 });

      Assert::AreEqual(size_t(1), text.Exposed.size());
      Assert::AreEqual(string("Command Prompt"), text.Exposed[0].Text);
      Assert::IsTrue(_exposedText->Windows == vector<WindowId>{ { 0x42 } });

      Assert::AreEqual(size_t(1), text.Recognized.size(), L"only the window's crop");
      Assert::AreEqual(string("Microsoft"), text.Recognized[0].Text);
      Assert::IsTrue(_recognizer->Regions == vector<Rect>{ { 100, 200, 300, 150 } });
    }

    TEST_METHOD(AnUnchangedWindowIsNotReadAgain)
    {
      auto screen = Screen(800, 600);
      Rect area{ 100, 200, 300, 150 };
      _reader.Read(screen, { 0x42 }, area);
      _reader.Read(screen, { 0x42 }, area);
      Assert::AreEqual(size_t(1), _exposedText->Windows.size());
      Assert::AreEqual(size_t(1), _recognizer->Regions.size());

      Change(screen, 10, 10);
      _reader.Read(screen, { 0x42 }, area);
      Assert::AreEqual(size_t(1), _recognizer->Regions.size(), L"changed outside the window");

      Change(screen, 150, 250);
      _reader.Read(screen, { 0x42 }, area);
      Assert::AreEqual(size_t(2), _exposedText->Windows.size());
      Assert::AreEqual(size_t(2), _recognizer->Regions.size());
    }
  };

  TEST_CLASS(ThoroughTextReaderTests)
  {
    shared_ptr<FakeRecognizer> _recognizer = make_shared<FakeRecognizer>();
    ThoroughTextReader _reader{ _recognizer };

  public:
    TEST_METHOD(TheScreenIsReadInBandsAndOverlapsMerged)
    {
      //The first is in the overlap of the first two bands, so both read it
      _recognizer->Words = { { "overlap", { 10, 260, 60, 16 } }, { "top", { 10, 10, 30, 16 } }, { "bottom", { 10, 1060, 50, 16 } } };

      auto words = _reader.Read(Screen(400, 1080), {});

      Assert::AreEqual(size_t(5), _recognizer->Regions.size(), L"one call per band");
      ranges::sort(words, {}, [](const ScreenWord& word) { return word.Box.Y; });
      Assert::AreEqual(size_t(3), words.size());
      Assert::AreEqual(string("top"), words[0].Text);
      Assert::AreEqual(string("overlap"), words[1].Text, L"once");
      Assert::AreEqual(string("bottom"), words[2].Text);
    }

    TEST_METHOD(OnlyBandsWhichChangedAreReadAgain)
    {
      auto screen = Screen(400, 1080);
      _reader.Read(screen, {});
      _reader.Read(screen, {});
      Assert::AreEqual(size_t(5), _recognizer->Regions.size(), L"a static screen costs no recognition");

      Change(screen, 200, 600);
      _reader.Read(screen, {});
      Assert::AreEqual(size_t(6), _recognizer->Regions.size());
      Assert::IsTrue(_recognizer->Regions.back() == Rect{ 0, 504, 400, 300 });
    }

    TEST_METHOD(AStoppedPassReadsNoMore)
    {
      stop_source stop;
      stop.request_stop();

      Assert::IsTrue(_reader.Read(Screen(400, 1080), stop.get_token()).empty());
      Assert::IsTrue(_recognizer->Regions.empty());
    }
  };
}
