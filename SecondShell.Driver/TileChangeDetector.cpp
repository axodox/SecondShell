#include "pch.h"
#include "TileChangeDetector.h"

using namespace std;

namespace SecondShell::Capture
{
  namespace
  {
    vector<Rect> Tiles(const Rect& screen)
    {
      vector<Rect> tiles;
      for (auto top = screen.Y; top < screen.Bottom(); top += DirtyRegion::TileSize)
      {
        for (auto left = screen.X; left < screen.Right(); left += DirtyRegion::TileSize)
        {
          tiles.push_back(Rect{ left, top, DirtyRegion::TileSize, DirtyRegion::TileSize }.Intersection(screen));
        }
      }
      return tiles;
    }
  }

  DirtyRegion TileChangeDetector::Compare(const Pixels& screen)
  {
    auto tiles = Tiles(screen.Bounds());
    vector<uint64_t> fingerprints;
    for (auto& tile : tiles) fingerprints.push_back(screen.Fingerprint(tile));

    DirtyRegion changes{ screen.Bounds() };
    if (_screen != screen.Bounds())
    {
      changes.AddAll();
    }
    else
    {
      for (size_t index = 0; index < tiles.size(); index++)
      {
        if (fingerprints[index] != _fingerprints[index]) changes.Add(tiles[index]);
      }
    }

    _screen = screen.Bounds();
    _fingerprints = move(fingerprints);
    return changes;
  }
}
