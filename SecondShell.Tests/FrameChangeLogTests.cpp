#include "pch.h"
#include "FrameChangeLog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(FrameChangeLogTests)
  {
    const Rect _screen{ 0, 0, 1920, 1080 };

  public:
    TEST_METHOD(ACheckTakesTheFramesItsSnapshotShows)
    {
      FrameChangeLog log;
      Assert::IsTrue(log.IsEmpty());

      vector<Rect> first{ { 100, 100, 10, 10 } }, second{ { 500, 500, 10, 10 } }, third{ { 900, 900, 10, 10 } };
      log.Add(1, _screen, first);
      log.Add(2, _screen, second);
      log.Add(3, _screen, third);

      auto changes = log.Take(2, _screen);
      Assert::IsTrue(changes.Region.Intersects(first[0]));
      Assert::IsTrue(changes.Region.Intersects(second[0]));
      Assert::IsFalse(changes.Region.Intersects(third[0]), L"after the snapshot");
      Assert::IsFalse(changes.HasUnreported);

      Assert::IsFalse(log.IsEmpty());
      Assert::IsTrue(log.Take(3, _screen).Region.Intersects(third[0]), L"for the next check");
      Assert::IsTrue(log.IsEmpty());
    }

    TEST_METHOD(FramesWithoutDirtyRegionsAreFlagged)
    {
      FrameChangeLog log;
      log.Add(1, _screen, {});

      auto changes = log.Take(1, _screen);
      Assert::IsTrue(changes.HasUnreported);
      Assert::IsTrue(changes.Region.IsEmpty());
    }

    TEST_METHOD(AFrameOfAnotherSizeChangesAll)
    {
      FrameChangeLog log;
      vector<Rect> corner{ { 0, 0, 1, 1 } };
      log.Add(1, { 0, 0, 1280, 720 }, corner);

      auto changes = log.Take(1, _screen);
      Assert::IsTrue(changes.Region.Intersects({ 1900, 1060, 1, 1 }));
    }
  };
}
