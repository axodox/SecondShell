#include "pch.h"
#include "AreaTextReader.h"
#include "FakeTextRecognizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(AreaTextReaderTests)
  {
    shared_ptr<FakeTextRecognizer> _recognizer = make_shared<FakeTextRecognizer>();
    AreaTextReader _reader{ _recognizer };
    vector<Rect> _areas{ { 0, 0, 800, 600 }, { 900, 100, 400, 300 }, { 0, 700, 1920, 300 } };

    static ScreenChanges Unknown(const ScreenFrame& frame)
    {
      DirtyRegion all{ frame.Bounds() };
      all.AddAll();
      return { move(all), nullptr };
    }

    static ScreenChanges Since(const shared_ptr<const ScreenFrame>& previous, const Rect& changed)
    {
      DirtyRegion region{ previous->Bounds() };
      region.Add(changed);
      return { move(region), previous };
    }

    static string Texts(const vector<ScreenWord>& words)
    {
      vector<ScreenWord> sorted = words;
      ranges::sort(sorted, {}, [](const ScreenWord& word) { return word.Box.Y; });

      string text;
      for (auto& word : sorted) text += (text.empty() ? "" : " ") + word.Text;
      return text;
    }

  public:
    TEST_METHOD(AllAreasAreReadInOneCall)
    {
      _recognizer->Words = { { "first", { 10, 10, 50, 16 } }, { "second", { 910, 110, 60, 16 } }, { "third", { 10, 710, 50, 16 } } };
      auto frame = Frame(1, 1920, 1080);

      auto texts = _reader.Read(frame, _areas, Unknown(*frame));

      Assert::AreEqual(size_t(1), _recognizer->Calls(), L"packed into one atlas");
      Assert::AreEqual(size_t(3), _recognizer->Atlases[0].Cells.size());
      Assert::AreEqual(string("first"), Texts(texts[0]));
      Assert::AreEqual(string("second"), Texts(texts[1]));
      Assert::AreEqual(string("third"), Texts(texts[2]));
      Assert::IsTrue(texts[1][0].Box == Rect{ 910, 110, 60, 16 }, L"boxes back on the screen");
    }

    TEST_METHOD(UnchangedAreasAreNotReadAgain)
    {
      _recognizer->Words = { { "first", { 10, 10, 50, 16 } } };
      auto frame = Frame(1, 1920, 1080);
      _reader.Read(frame, _areas, Unknown(*frame));

      auto texts = _reader.Read(Frame(2, 1920, 1080), _areas, Unknown(*frame));

      Assert::AreEqual(size_t(1), _recognizer->Calls());
      Assert::AreEqual(string("first"), Texts(texts[0]), L"from what was kept");
    }

    TEST_METHOD(OnlyTheLinesWhichChangedAreReadAgain)
    {
      _recognizer->Words = { { "kept", { 10, 100, 50, 16 } }, { "before", { 10, 300, 50, 16 } } };
      auto first = Frame(1, 1920, 1080);
      _reader.Read(first, _areas, Unknown(*first));

      _recognizer->Words = { { "stale", { 10, 100, 50, 16 } }, { "after", { 10, 300, 50, 16 } } };
      auto second = Changed(*first, 20, 305);
      auto texts = _reader.Read(second, _areas, Since(first, { 20, 305, 1, 1 }));

      Assert::AreEqual(size_t(2), _recognizer->Calls());
      Assert::IsTrue(_recognizer->Atlases[1].Cells.size() == 1 && _recognizer->Atlases[1].Cells[0].Source == Rect{ 0, 264, 800, 80 }, L"the changed line, a line around it");
      Assert::AreEqual(string("kept after"), Texts(texts[0]));
    }

    TEST_METHOD(AreasAreReadWholeWhenTheChangesStartElsewhere)
    {
      _recognizer->Words = { { "before", { 10, 300, 50, 16 } } };
      auto first = Frame(1, 1920, 1080);
      _reader.Read(first, _areas, Unknown(*first));

      //The changes start from a frame other than the one read: what changed between them is not known
      auto other = Frame(5, 1920, 1080, 0);
      auto second = Changed(*first, 20, 305);
      _reader.Read(second, _areas, Since(other, { 20, 305, 1, 1 }));

      Assert::IsTrue(_recognizer->Read().back() == _areas[0]);
    }

    TEST_METHOD(AtlasesStayWithinTheRecognizersSize)
    {
      _recognizer->MaxSide = 1000;
      auto frame = Frame(1, 1920, 1080);

      _reader.Read(frame, _areas, Unknown(*frame));

      Assert::AreEqual(size_t(2), _recognizer->Calls(), L"the widest gets an atlas of its own");
      Assert::AreEqual(size_t(1), _recognizer->Atlases[1].Cells.size());
    }

    TEST_METHOD(AStoppedReadingReadsNothing)
    {
      stop_source stop;
      stop.request_stop();
      auto frame = Frame(1, 1920, 1080);

      auto texts = _reader.Read(frame, _areas, Unknown(*frame), stop.get_token());

      Assert::AreEqual(size_t(0), _recognizer->Calls());
      Assert::IsTrue(texts[0].empty());
    }
  };
}
