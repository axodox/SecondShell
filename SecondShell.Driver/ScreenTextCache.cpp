#include "pch.h"
#include "ScreenTextCache.h"

using namespace std;

namespace SecondShell::Capture
{
  ScreenTextCache::ScreenTextCache(size_t capacity) :
    _capacity(capacity)
  { }

  vector<ScreenWord> ScreenTextCache::Read(const Rect& area, uint64_t fingerprint, const function<vector<ScreenWord>()>& read)
  {
    if (auto words = Find(area, fingerprint)) return *words;

    //Not locked while reading, which takes long
    auto words = read();
    Store(area, fingerprint, words);
    return words;
  }

  optional<vector<ScreenWord>> ScreenTextCache::Find(const Rect& area, uint64_t fingerprint)
  {
    lock_guard lock{ _mutex };
    auto entry = ranges::find(_entries, area, &Entry::Area);
    if (entry == _entries.end() || entry->Fingerprint != fingerprint) return nullopt;

    entry->LastUse = ++_uses;
    return entry->Words;
  }

  void ScreenTextCache::Store(const Rect& area, uint64_t fingerprint, const vector<ScreenWord>& words)
  {
    lock_guard lock{ _mutex };
    auto entry = ranges::find(_entries, area, &Entry::Area);
    if (entry == _entries.end())
    {
      if (_entries.size() >= _capacity) _entries.erase(ranges::min_element(_entries, {}, &Entry::LastUse));
      entry = _entries.insert(_entries.end(), { area });
    }

    entry->Fingerprint = fingerprint;
    entry->Words = words;
    entry->LastUse = ++_uses;
  }
}
