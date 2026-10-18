#pragma once

namespace SecondShell::Capture
{
  //A rectangle of the screen, in session pixels.
  struct Rect
  {
    int32_t X = 0;
    int32_t Y = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;

    int32_t Right() const { return X + int32_t(Width); }
    int32_t Bottom() const { return Y + int32_t(Height); }
    bool IsEmpty() const { return Width == 0 || Height == 0; }

    //Empty if they do not overlap.
    Rect Intersection(const Rect& other) const
    {
      auto left = std::max(X, other.X);
      auto top = std::max(Y, other.Y);
      auto right = std::min(Right(), other.Right());
      auto bottom = std::min(Bottom(), other.Bottom());
      if (right <= left || bottom <= top) return {};
      return { left, top, uint32_t(right - left), uint32_t(bottom - top) };
    }

    bool Intersects(const Rect& other) const { return !Intersection(other).IsEmpty(); }

    bool operator==(const Rect&) const = default;
  };
}
