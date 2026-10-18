#pragma once
#include "Rect.h"

namespace SecondShell::Capture
{
  //The parts of the screen which changed, as tiles: bounded in size however many rectangles are added.
  class DirtyRegion
  {
  public:
    static constexpr uint32_t TileSize = 32;

    explicit DirtyRegion(const Rect& screen = {});

    const Rect& Screen() const;
    bool IsEmpty() const;
    bool Intersects(const Rect& rect) const;

    //The runs of tile rows which changed within the area, each as wide as the area and clipped to it.
    std::vector<Rect> RowsWithin(const Rect& area) const;

    //Clipped to the screen.
    void Add(const Rect& rect);
    //All of it, if the other region is of another screen (a resolution change).
    void Add(const DirtyRegion& other);
    void AddAll();
    void Clear();

  private:
    //Columns and rows of tiles, the ends exclusive
    struct TileSpan
    {
      uint32_t Left, Top, Right, Bottom;
    };

    Rect _screen;
    uint32_t _columns = 0;
    uint32_t _rows = 0;
    std::vector<bool> _tiles;
    size_t _count = 0;

    //The tiles the rectangle touches; none outside the screen.
    TileSpan TilesOf(const Rect& rect) const;
  };
}
