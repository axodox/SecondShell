#include "pch.h"
#include "Pixels.h"

using namespace std;

namespace SecondShell::Capture
{
  namespace
  {
    const uint64_t _multiplier = 0x9E3779B97F4A7C15ull;

    //Eight bytes at a time, as this runs over whole screens while a wait polls
    uint64_t Mix(uint64_t hash, span<const uint8_t> bytes)
    {
      size_t index = 0;
      for (; index + 8 <= bytes.size(); index += 8)
      {
        uint64_t chunk;
        memcpy(&chunk, bytes.data() + index, 8);
        hash = rotl((hash ^ chunk) * _multiplier, 29);
      }

      for (; index < bytes.size(); index++) hash = rotl((hash ^ bytes[index]) * _multiplier, 29);
      return hash;
    }
  }

  Pixels Pixels::Crop(const Rect& area) const
  {
    Pixels result{ area.Width, area.Height, vector<uint8_t>(size_t(area.Width) * area.Height * 4) };
    auto rowSize = size_t(area.Width) * 4;
    for (uint32_t row = 0; row < area.Height; row++)
    {
      memcpy(result.Bgra.data() + row * rowSize, Bgra.data() + (size_t(area.Y + row) * Width + area.X) * 4, rowSize);
    }
    return result;
  }

  uint64_t Pixels::Fingerprint(const Rect& area) const
  {
    uint64_t hash = 0;
    auto rowSize = size_t(area.Width) * 4;
    for (uint32_t row = 0; row < area.Height; row++)
    {
      hash = Mix(hash, { Bgra.data() + (size_t(area.Y + row) * Width + area.X) * 4, rowSize });
    }
    return hash;
  }

  uint64_t Pixels::Fingerprint() const
  {
    return Fingerprint(Bounds());
  }

  Rect Pixels::Bounds() const
  {
    return { 0, 0, Width, Height };
  }
}
