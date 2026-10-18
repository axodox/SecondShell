#include "pch.h"
#include "ScreenTextCache.h"

using namespace std;

namespace SecondShell::Capture
{
  ScreenTextCache::ScreenTextCache(size_t capacity) :
    _capacity(capacity)
  { }

  optional<AreaText> ScreenTextCache::Find(const Rect& area)
  {
    lock_guard lock{ _mutex };
    auto entry = ranges::find(_entries, area, &Entry::Area);
    if (entry == _entries.end()) return nullopt;

    entry->LastUse = ++_uses;
    return entry->Text;
  }

  void ScreenTextCache::Store(const Rect& area, AreaText text)
  {
    lock_guard lock{ _mutex };
    auto entry = ranges::find(_entries, area, &Entry::Area);
    if (entry == _entries.end())
    {
      if (_entries.size() >= _capacity) _entries.erase(ranges::min_element(_entries, {}, &Entry::LastUse));
      entry = _entries.insert(_entries.end(), { area });
    }

    entry->Text = move(text);
    entry->LastUse = ++_uses;
  }
}
