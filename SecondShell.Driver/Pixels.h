#pragma once
#include "Rect.h"

namespace SecondShell::Capture
{
  //Pixels of a region, 32-bit BGRA, rows without padding.
  struct Pixels
  {
    uint32_t Width = 0;
    uint32_t Height = 0;
    std::vector<uint8_t> Bgra;

    //A copy of the area, which must be inside.
    Pixels Crop(const Rect& area) const;

    //A hash of the area's pixels, to tell whether they changed; the area must be inside.
    uint64_t Fingerprint(const Rect& area) const;
    uint64_t Fingerprint() const;

    Rect Bounds() const;
  };
}
