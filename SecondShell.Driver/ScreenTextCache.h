#pragma once
#include "ScreenWord.h"

namespace SecondShell::Capture
{
  //What an area of the screen read, and the fingerprint of the pixels it read it from.
  struct AreaText
  {
    uint64_t Fingerprint = 0;
    std::vector<ScreenWord> Words;
  };

  //What areas of the screen read, kept between waits, so that an area is read again only once its pixels change, and
  //a wait for text already on an unchanged screen answers at once. Holds the most recently used areas only.
  class ScreenTextCache
  {
  public:
    explicit ScreenTextCache(size_t capacity = 64);

    std::optional<AreaText> Find(const Rect& area);
    void Store(const Rect& area, AreaText text);

  private:
    struct Entry
    {
      Rect Area;
      AreaText Text;
      uint64_t LastUse;
    };

    size_t _capacity;
    std::mutex _mutex;
    std::vector<Entry> _entries;
    uint64_t _uses = 0;
  };
}
