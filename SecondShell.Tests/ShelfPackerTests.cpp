#include "pch.h"
#include "ShelfPacker.h"
#include "TextAtlas.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ShelfPackerTests)
  {
    static const uint32_t _gutter = 20;

    //Inside its atlas and at least a gutter from its edges and from every other cell
    static void AssertSeparated(const AtlasLayout& atlas)
    {
      for (auto& cell : atlas.Cells)
      {
        auto placed = cell.Placed();
        Assert::IsTrue(placed.X >= int32_t(_gutter) && placed.Y >= int32_t(_gutter), L"a gutter before");
        Assert::IsTrue(placed.Right() + int32_t(_gutter) <= int32_t(atlas.Width) && placed.Bottom() + int32_t(_gutter) <= int32_t(atlas.Height), L"a gutter after");

        Rect withGutter{ placed.X - int32_t(_gutter), placed.Y - int32_t(_gutter), placed.Width + 2 * _gutter, placed.Height + 2 * _gutter };
        for (auto& other : atlas.Cells)
        {
          if (&other != &cell) Assert::IsFalse(withGutter.Intersects(other.Placed()), L"a gutter between");
        }
      }
    }

    static vector<size_t> Indexes(const vector<AtlasLayout>& atlases)
    {
      vector<size_t> indexes;
      for (auto& atlas : atlases)
      {
        for (auto& cell : atlas.Cells) indexes.push_back(cell.Index);
      }
      ranges::sort(indexes);
      return indexes;
    }

  public:
    TEST_METHOD(RectanglesArePackedTallestFirstInRows)
    {
      vector<Rect> strips{ { 0, 0, 1000, 60 }, { 0, 100, 1000, 60 }, { 0, 300, 500, 100 }, { 0, 500, 1000, 60 }, { 0, 700, 300, 30 } };

      auto atlases = ShelfPacker::Pack(strips, 2500, _gutter);

      Assert::AreEqual(size_t(1), atlases.size());
      auto& atlas = atlases[0];
      Assert::IsTrue(atlas.Width <= 2500 && atlas.Height <= 2500);
      Assert::AreEqual(size_t(2), atlas.Cells[0].Index, L"the tallest first");
      Assert::AreEqual(int32_t(_gutter), atlas.Cells[0].X);
      Assert::AreEqual(int32_t(_gutter), atlas.Cells[0].Y);
      Assert::IsTrue(atlas.Height < 400, L"two rows");
      AssertSeparated(atlas);
      Assert::IsTrue(Indexes(atlases) == vector<size_t>{ 0, 1, 2, 3, 4 });
    }

    TEST_METHOD(AtlasesStayWithinTheMaximumSide)
    {
      vector<Rect> bands(10, Rect{ 0, 0, 900, 400 });

      auto atlases = ShelfPacker::Pack(bands, 2000, _gutter);

      Assert::IsTrue(atlases.size() > 1);
      for (auto& atlas : atlases)
      {
        Assert::IsTrue(atlas.Width <= 2000 && atlas.Height <= 2000);
        AssertSeparated(atlas);
      }
      Assert::AreEqual(size_t(10), Indexes(atlases).size());
    }

    TEST_METHOD(ARectangleTooLargeGetsAnAtlasOfItsOwn)
    {
      vector<Rect> rectangles{ { 0, 0, 100, 20 }, { 0, 0, 3000, 50 }, { 0, 0, 100, 20 } };

      auto atlases = ShelfPacker::Pack(rectangles, 2000, _gutter);

      Assert::AreEqual(size_t(2), atlases.size());
      Assert::AreEqual(size_t(1), atlases[0].Cells.size());
      Assert::AreEqual(size_t(1), atlases[0].Cells[0].Index);
      Assert::AreEqual(size_t(2), atlases[1].Cells.size());
    }

    TEST_METHOD(NothingToPackIsNoAtlas)
    {
      Assert::IsTrue(ShelfPacker::Pack({}, 2000, _gutter).empty());
    }

    TEST_METHOD(BoxesMapBackThroughTheCellHoldingTheirCentre)
    {
      AtlasLayout atlas{ 600, 80, { { 0, { 100, 200, 300, 40 }, 20, 20 }, { 1, { 50, 600, 200, 40 }, 340, 20 } } };

      auto located = atlas.Locate({ 350, 25, 50, 16 });
      Assert::IsTrue(located.has_value());
      Assert::AreEqual(size_t(1), located->Index);
      Assert::IsTrue(located->Box == Rect{ 60, 605, 50, 16 });

      located = atlas.Locate({ 18, 30, 40, 16 });
      Assert::IsTrue(located.has_value(), L"a pixel or two over the edge is the engine's slack");
      Assert::IsTrue(located->Box == Rect{ 100, 210, 38, 16 }, L"clipped to the cell");
    }

    TEST_METHOD(BoxesAcrossCellsOrInGuttersAreDropped)
    {
      AtlasLayout atlas{ 600, 80, { { 0, { 100, 200, 300, 40 }, 20, 20 }, { 1, { 50, 600, 200, 40 }, 340, 20 } } };

      Assert::IsFalse(atlas.Locate({ 300, 25, 60, 16 }).has_value(), L"over the gutter");
      Assert::IsFalse(atlas.Locate({ 322, 25, 14, 16 }).has_value(), L"in the gutter");
      Assert::IsFalse(atlas.Locate({ 300, 25, 100, 16 }).has_value(), L"centre in a cell, but reaching into the next");
    }
  };

  TEST_CLASS(TextAtlasTests)
  {
  public:
    TEST_METHOD(TheAtlasIsComposedFromTheFramesPixels)
    {
      Pixels screen{ 8, 4, vector<uint8_t>(8 * 4 * 4) };
      for (size_t index = 0; index < screen.Bgra.size(); index++) screen.Bgra[index] = uint8_t(index);
      auto frame = make_shared<ScreenFrame>(1, screen);

      TextAtlas atlas{ frame, { 6, 5, { { 0, { 5, 1, 2, 2 }, 1, 2 } } } };
      auto pixels = atlas.CpuPixels();

      Assert::AreEqual(6u, pixels.Width);
      Assert::AreEqual(5u, pixels.Height);
      Assert::AreEqual(TextAtlas::Background, pixels.Bgra[0], L"a flat gutter");
      Assert::AreEqual(screen.Bgra[(1 * 8 + 5) * 4], pixels.Bgra[(2 * 6 + 1) * 4]);
      Assert::AreEqual(screen.Bgra[(2 * 8 + 6) * 4 + 3], pixels.Bgra[(3 * 6 + 2) * 4 + 3]);
    }

    TEST_METHOD(AFrameIsReadBackOnceAndOnlyWhenAsked)
    {
      auto readBacks = 0;
      ScreenFrame frame{ 7, nullptr, { 0, 0, 2, 2 }, [&] {
        readBacks++;
        return Pixels{ 2, 2, vector<uint8_t>(16, 1) };
        } };

      Assert::AreEqual(0, readBacks);
      Assert::AreEqual(2u, frame.CpuPixels().Width);
      Assert::AreEqual(2u, frame.CpuPixels().Height);
      Assert::AreEqual(1, readBacks);
      Assert::IsTrue(frame.Bounds() == Rect{ 0, 0, 2, 2 });
    }
  };
}
