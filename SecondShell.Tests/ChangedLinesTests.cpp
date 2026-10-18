#include "pch.h"
#include "ChangedLines.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ChangedLinesTests)
  {
  public:
    TEST_METHOD(ChangedRowsSpanTheArea)
    {
      DirtyRegion changes{ { 0, 0, 1920, 1080 } };
      changes.Add({ 500, 100, 10, 10 });
      changes.Add({ 520, 140, 10, 10 });

      auto rows = changes.RowsWithin({ 100, 0, 800, 1000 });
      Assert::AreEqual(size_t(1), rows.size(), L"consecutive tile rows are one run");
      Assert::IsTrue(rows[0] == Rect{ 100, 96, 800, 64 });

      Assert::IsTrue(changes.RowsWithin({ 1000, 0, 800, 1000 }).empty(), L"the change is not in this area's columns");
    }

    TEST_METHOD(StripsGrowByALineToTheAreasWidth)
    {
      DirtyRegion changes{ { 0, 0, 1920, 1080 } };
      changes.Add({ 300, 300, 5, 5 });

      auto strips = ChangedLines::Of({ 100, 0, 800, 600 }, changes);

      Assert::AreEqual(size_t(1), strips.size());
      Assert::IsTrue(strips[0].Changed == Rect{ 100, 288, 800, 32 });
      Assert::IsTrue(strips[0].Read == Rect{ 100, 264, 800, 80 });
    }

    TEST_METHOD(StripsWhichMeetAreMergedAndClippedToTheArea)
    {
      DirtyRegion changes{ { 0, 0, 1920, 1080 } };
      changes.Add({ 300, 5, 5, 5 });
      changes.Add({ 300, 70, 5, 5 });
      changes.Add({ 300, 500, 5, 5 });

      auto strips = ChangedLines::Of({ 100, 0, 800, 600 }, changes);

      Assert::AreEqual(size_t(2), strips.size());
      Assert::IsTrue(strips[0].Read == Rect{ 100, 0, 800, 120 }, L"the first two meet once grown, and the area's top clips them");
      Assert::IsTrue(strips[0].Changed == Rect{ 100, 0, 800, 96 });
      Assert::IsTrue(strips[1].Read == Rect{ 100, 456, 800, 80 });
    }

    TEST_METHOD(AnUnchangedAreaHasNoStrips)
    {
      DirtyRegion changes{ { 0, 0, 1920, 1080 } };
      changes.Add({ 1500, 500, 5, 5 });

      Assert::IsTrue(ChangedLines::Of({ 100, 0, 800, 600 }, changes).empty());
    }
  };
}
