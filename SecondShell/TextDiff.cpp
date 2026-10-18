#include "pch.h"
#include "TextDiff.h"

using namespace std;

namespace SecondShell::Integrations
{
  namespace
  {
    vector<string_view> Lines(string_view text)
    {
      vector<string_view> lines;
      while (!text.empty())
      {
        auto end = text.find('\n');
        auto line = text.substr(0, end);
        if (line.ends_with('\r')) line.remove_suffix(1);
        lines.push_back(line);
        if (end == string_view::npos) break;
        text.remove_prefix(end + 1);
      }
      return lines;
    }
  }

  vector<DiffLine> ChangedLines(string_view before, string_view after, size_t context)
  {
    auto old = Lines(before);
    auto changed = Lines(after);

    //The lines both start and end with
    size_t prefix = 0;
    while (prefix < old.size() && prefix < changed.size() && old[prefix] == changed[prefix]) prefix++;
    size_t suffix = 0;
    while (suffix < old.size() - prefix && suffix < changed.size() - prefix && old[old.size() - 1 - suffix] == changed[changed.size() - 1 - suffix]) suffix++;

    vector<DiffLine> result;
    auto contextStart = prefix > context ? prefix - context : 0;
    if (contextStart > 0) result.push_back({ '~', {} });
    for (auto index = contextStart; index < prefix; index++) result.push_back({ ' ', string{ old[index] } });
    for (auto index = prefix; index < old.size() - suffix; index++) result.push_back({ '-', string{ old[index] } });
    for (auto index = prefix; index < changed.size() - suffix; index++) result.push_back({ '+', string{ changed[index] } });

    auto contextEnd = min(changed.size(), changed.size() - suffix + context);
    for (auto index = changed.size() - suffix; index < contextEnd; index++) result.push_back({ ' ', string{ changed[index] } });
    if (contextEnd < changed.size()) result.push_back({ '~', {} });
    return result;
  }
}
