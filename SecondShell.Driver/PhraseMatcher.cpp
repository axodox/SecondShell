#include "pch.h"
#include "PhraseMatcher.h"
#include "TextLines.h"

using namespace std;

namespace SecondShell::Capture
{
  namespace
  {
    //Compatibility forms as their plain characters, e.g. full-width letters and ligatures
    wstring NormalizeCompatibility(const wstring& text)
    {
      if (text.empty()) return text;

      auto size = NormalizeString(NormalizationKC, text.data(), int(text.size()), nullptr, 0);
      wstring result(size_t(max(size, 0)), L'\0');
      size = NormalizeString(NormalizationKC, text.data(), int(text.size()), result.data(), int(result.size()));
      if (size <= 0) return text;

      result.resize(size_t(size));
      return result;
    }

    wchar_t LookAlike(wchar_t character)
    {
      switch (character)
      {
      case L'i':
      case L'1':
      case L'|':
        return L'l';
      case L'0':
        return L'o';
      default:
        return character;
      }
    }

    //A fifth, none for the shortest phrases, which would otherwise match other short words
    size_t AllowedEdits(size_t length)
    {
      return length > 0 ? (length - 1) / 5 : 0;
    }
  }

  PhraseMatcher::PhraseMatcher(string_view phrase) :
    _phrase(Normalize(phrase)),
    _allowedEdits(AllowedEdits(_phrase.size()))
  { }

  bool PhraseMatcher::IsIn(span<const ScreenWord> words) const
  {
    string text;
    for (auto& line : TextLines::FromWords(words)) text += line + '\n';
    return IsIn(text);
  }

  bool PhraseMatcher::IsIn(string_view text) const
  {
    return SubstringDistance(_phrase, Normalize(text)) <= _allowedEdits;
  }

  wstring PhraseMatcher::Normalize(string_view text)
  {
    //All of Unicode, unlike towlower in the C locale
    auto normalized = NormalizeCompatibility(wstring{ winrt::to_hstring(text) });
    CharLowerBuffW(normalized.data(), DWORD(normalized.size()));

    wstring result;
    for (auto character : normalized)
    {
      if (iswspace(character)) continue;

      character = LookAlike(character);
      if (character == L'n' && result.ends_with(L'r'))
      {
        result.back() = L'm';
      }
      else
      {
        result += character;
      }
    }
    return result;
  }

  size_t PhraseMatcher::SubstringDistance(wstring_view pattern, wstring_view text)
  {
    //Edits of the pattern's prefixes against substrings ending at the current character, which may start anywhere
    vector<size_t> previous(pattern.size() + 1);
    iota(previous.begin(), previous.end(), size_t(0));
    vector<size_t> current(pattern.size() + 1);

    auto best = previous.back();
    for (auto character : text)
    {
      current[0] = 0;
      for (size_t index = 1; index <= pattern.size(); index++)
      {
        current[index] = min({ previous[index] + 1, current[index - 1] + 1, previous[index - 1] + (pattern[index - 1] == character ? 0 : 1) });
      }

      best = min(best, current.back());
      swap(previous, current);
    }
    return best;
  }
}
