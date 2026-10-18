#pragma once
#include "ScreenWord.h"

namespace SecondShell::Capture
{
  //What areas of the screen read, with the fingerprints of their pixels: an area is read again only once its pixels
  //change. Kept between waits, so a wait for text already on an unchanged screen answers at once. Holds the most
  //recently used areas only.
  class ScreenTextCache
  {
  public:
    explicit ScreenTextCache(size_t capacity = 64);

    //The words the area read with these pixels, reading them if they are not known.
    std::vector<ScreenWord> Read(const Rect& area, uint64_t fingerprint, const std::function<std::vector<ScreenWord>()>& read);

  private:
    struct Entry
    {
      Rect Area;
      uint64_t Fingerprint;
      std::vector<ScreenWord> Words;
      uint64_t LastUse;
    };

    size_t _capacity;
    std::mutex _mutex;
    std::vector<Entry> _entries;
    uint64_t _uses = 0;

    std::optional<std::vector<ScreenWord>> Find(const Rect& area, uint64_t fingerprint);
    void Store(const Rect& area, uint64_t fingerprint, const std::vector<ScreenWord>& words);
  };
}
