#include "pch.h"
#include "ShelfPacker.h"

using namespace std;

namespace SecondShell::Capture
{
  namespace
  {
    //Fills one atlas after the other; positions include the gutter before each cell
    class Shelves
    {
    public:
      Shelves(uint32_t maxSide, uint32_t gutter) :
        _maxSide(maxSide),
        _gutter(gutter)
      {
        NewAtlas();
      }

      void Place(size_t index, const Rect& rectangle)
      {
        auto isOversized = rectangle.Width + 2 * _gutter > _maxSide || rectangle.Height + 2 * _gutter > _maxSide;
        if (isOversized && !IsAtlasEmpty()) NewAtlas();
        if (_x + rectangle.Width + _gutter > _maxSide && !IsShelfEmpty()) NewShelf();
        if (_y + rectangle.Height + _gutter > _maxSide && !IsAtlasEmpty()) NewAtlas();

        auto& atlas = _atlases.back();
        atlas.Cells.push_back({ index, rectangle, int32_t(_x), int32_t(_y) });
        _x += rectangle.Width + _gutter;
        _shelfHeight = max(_shelfHeight, rectangle.Height);
        atlas.Width = max(atlas.Width, _x);
        atlas.Height = max(atlas.Height, _y + _shelfHeight + _gutter);
        if (isOversized) NewAtlas();
      }

      vector<AtlasLayout> Atlases() &&
      {
        if (IsAtlasEmpty()) _atlases.pop_back();
        return move(_atlases);
      }

    private:
      uint32_t _maxSide;
      uint32_t _gutter;
      vector<AtlasLayout> _atlases;
      uint32_t _x = 0;
      uint32_t _y = 0;
      uint32_t _shelfHeight = 0;

      bool IsShelfEmpty() const { return _x == _gutter; }
      bool IsAtlasEmpty() const { return _atlases.back().Cells.empty(); }

      void NewShelf()
      {
        _x = _gutter;
        _y += _shelfHeight + _gutter;
        _shelfHeight = 0;
      }

      void NewAtlas()
      {
        _atlases.emplace_back();
        _x = _gutter;
        _y = _gutter;
        _shelfHeight = 0;
      }
    };
  }

  vector<AtlasLayout> ShelfPacker::Pack(span<const Rect> rectangles, uint32_t maxSide, uint32_t gutter)
  {
    vector<size_t> tallestFirst(rectangles.size());
    iota(tallestFirst.begin(), tallestFirst.end(), size_t(0));
    ranges::stable_sort(tallestFirst, greater{}, [&](size_t index) { return rectangles[index].Height; });

    Shelves shelves{ maxSide, gutter };
    for (auto index : tallestFirst) shelves.Place(index, rectangles[index]);
    return move(shelves).Atlases();
  }
}
