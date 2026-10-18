#include "pch.h"
#include "DirtyRegion.h"

using namespace std;

namespace SecondShell::Capture
{
  DirtyRegion::DirtyRegion(const Rect& screen) :
    _screen(screen),
    _columns((screen.Width + TileSize - 1) / TileSize),
    _rows((screen.Height + TileSize - 1) / TileSize),
    _tiles(size_t(_columns) * _rows)
  { }

  const Rect& DirtyRegion::Screen() const
  {
    return _screen;
  }

  bool DirtyRegion::IsEmpty() const
  {
    return _count == 0;
  }

  bool DirtyRegion::Intersects(const Rect& rect) const
  {
    auto tiles = TilesOf(rect);
    for (auto row = tiles.Top; row < tiles.Bottom; row++)
    {
      for (auto column = tiles.Left; column < tiles.Right; column++)
      {
        if (_tiles[size_t(row) * _columns + column]) return true;
      }
    }
    return false;
  }

  void DirtyRegion::Add(const Rect& rect)
  {
    auto tiles = TilesOf(rect);
    for (auto row = tiles.Top; row < tiles.Bottom; row++)
    {
      for (auto column = tiles.Left; column < tiles.Right; column++)
      {
        auto tile = _tiles[size_t(row) * _columns + column];
        if (!tile)
        {
          tile = true;
          _count++;
        }
      }
    }
  }

  void DirtyRegion::Add(const DirtyRegion& other)
  {
    if (other._screen != _screen)
    {
      *this = DirtyRegion{ other._screen };
      AddAll();
      return;
    }

    for (size_t index = 0; index < _tiles.size(); index++)
    {
      if (other._tiles[index] && !_tiles[index])
      {
        _tiles[index] = true;
        _count++;
      }
    }
  }

  void DirtyRegion::AddAll()
  {
    Add(_screen);
  }

  void DirtyRegion::Clear()
  {
    _tiles.assign(_tiles.size(), false);
    _count = 0;
  }

  DirtyRegion::TileSpan DirtyRegion::TilesOf(const Rect& rect) const
  {
    auto area = rect.Intersection(_screen);
    if (area.IsEmpty()) return {};

    auto left = uint32_t(area.X - _screen.X);
    auto top = uint32_t(area.Y - _screen.Y);
    return {
      left / TileSize,
      top / TileSize,
      (left + area.Width + TileSize - 1) / TileSize,
      (top + area.Height + TileSize - 1) / TileSize
    };
  }
}
