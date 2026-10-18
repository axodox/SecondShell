#pragma once

namespace SecondShell::Input
{
  //A position in session pixels.
  struct Point
  {
    int32_t X = 0;
    int32_t Y = 0;

    bool operator==(const Point&) const = default;
  };
}
