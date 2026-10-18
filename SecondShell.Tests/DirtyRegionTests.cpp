#include "pch.h"
#include "DirtyRegion.h"
#include "TileChangeDetector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(DirtyRegionTests)
  {
    static Pixels Gray(uint32_t width, uint32_t height)
    {
      return { width, height, vector<uint8_t>(size_t(width) * height * 4, 128) };
    }

    static void SetPixel(Pixels& pixels, uint32_t x, uint32_t y)
    {
      pixels.Bgra[(size_t(y) * pixels.Width + x) * 4] = 0;
    }

  public:
    TEST_METHOD(RectanglesAccumulateAsTiles)
    {
      DirtyRegion region{ { 0, 0, 1920, 1080 } };
      Assert::IsTrue(region.IsEmpty());

      region.Add({ 100, 100, 10, 10 });
      region.Add({ 1000, 600, 300, 20 });
      Assert::IsFalse(region.IsEmpty());
      Assert::IsTrue(region.Intersects({ 0, 0, 105, 105 }));
      Assert::IsTrue(region.Intersects({ 120, 120, 5, 5 }), L"the same tile");
      Assert::IsTrue(region.Intersects({ 1200, 610, 1, 1 }));
      Assert::IsFalse(region.Intersects({ 500, 500, 100, 50 }));
    }

    TEST_METHOD(RectanglesAreClippedToTheScreen)
    {
      DirtyRegion region{ { 0, 0, 100, 100 } };
      region.Add({ 200, 200, 10, 10 });
      region.Add({ -50, -50, 10, 10 });
      Assert::IsTrue(region.IsEmpty());

      region.Add({ 90, 90, 50, 50 });
      Assert::IsTrue(region.Intersects({ 99, 99, 1, 1 }));
    }

    TEST_METHOD(RegionsMergeAndClear)
    {
      DirtyRegion region{ { 0, 0, 640, 480 } };
      DirtyRegion other{ { 0, 0, 640, 480 } };
      other.Add({ 300, 300, 1, 1 });

      region.Add(other);
      Assert::IsTrue(region.Intersects({ 300, 300, 1, 1 }));

      region.Clear();
      Assert::IsTrue(region.IsEmpty());
    }

    TEST_METHOD(ARegionOfAnotherScreenSizeChangesAll)
    {
      DirtyRegion region{ { 0, 0, 640, 480 } };
      region.Add(DirtyRegion{ { 0, 0, 800, 600 } });

      Assert::IsTrue(region.Screen() == Rect{ 0, 0, 800, 600 });
      Assert::IsTrue(region.Intersects({ 0, 0, 1, 1 }));
      Assert::IsTrue(region.Intersects({ 799, 599, 1, 1 }));
    }

    TEST_METHOD(TilesWhosePixelsChangedAreFound)
    {
      TileChangeDetector detector;
      auto screen = Gray(200, 100);
      Assert::IsTrue(detector.Compare(screen).Intersects({ 199, 99, 1, 1 }), L"all of it at first");
      Assert::IsTrue(detector.Compare(screen).IsEmpty(), L"nothing changed");

      SetPixel(screen, 70, 40);
      auto changes = detector.Compare(screen);
      Assert::IsTrue(changes.Intersects({ 70, 40, 1, 1 }));
      Assert::IsFalse(changes.Intersects({ 0, 0, 64, 100 }));
      Assert::IsFalse(changes.Intersects({ 96, 0, 104, 100 }));

      Assert::IsTrue(detector.Compare(Gray(100, 100)).Intersects({ 0, 0, 1, 1 }), L"all of it after a resolution change");
    }
  };
}
