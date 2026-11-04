#include "pch.h"
#include "AnsiText.h"

using namespace std;

namespace SecondShell::Text
{
  namespace
  {
    //Windows Terminal's default scheme, Campbell
    const uint32_t _campbell[16] = {
      0x0C0C0C, 0xC50F1F, 0x13A10E, 0xC19C00, 0x0037DA, 0x881798, 0x3A96DD, 0xCCCCCC,
      0x767676, 0xE74856, 0x16C60C, 0xF9F1A5, 0x3B78FF, 0xB4009E, 0x61D6D6, 0xF2F2F2
    };

    const char _escape = '\x1B';

    //The length of the escape code at the start of the text; 0 if it is not complete yet
    size_t CodeLength(string_view text, string_view& parameters, char& final)
    {
      final = 0;
      if (text.size() < 2) return 0;

      switch (text[1])
      {
      case '[':
      {
        //Control sequence: parameters and intermediates, then a final byte
        for (size_t index = 2; index < text.size(); index++)
        {
          auto character = uint8_t(text[index]);
          if (character >= 0x40 && character <= 0x7E)
          {
            parameters = text.substr(2, index - 2);
            final = char(character);
            return index + 1;
          }
          if (character < 0x20 || character > 0x3F) return index;  //not a sequence after all: drop what was read
        }
        return text.size();
      }
      case ']':
      case 'P':
      case '_':
      case '^':
      {
        //A string, e.g. the window title: until BEL or ST
        for (size_t index = 2; index < text.size(); index++)
        {
          if (text[index] == '\a') return index + 1;
          if (text[index] == _escape && index + 1 < text.size() && text[index + 1] == '\\') return index + 2;
        }
        return text.size();
      }
      default:
        //Two characters, or three for a character set choice such as ESC ( B
        return text[1] == '(' || text[1] == ')' ? min<size_t>(3, text.size()) : 2;
      }
    }

    vector<int> Numbers(string_view parameters)
    {
      vector<int> numbers;
      int value = 0;
      auto hasValue = false;
      for (auto character : parameters)
      {
        if (character >= '0' && character <= '9')
        {
          value = min(value * 10 + (character - '0'), 100000);
          hasValue = true;
        }
        else if (character == ';' || character == ':')
        {
          numbers.push_back(hasValue ? value : 0);
          value = 0;
          hasValue = false;
        }
      }
      numbers.push_back(hasValue ? value : 0);
      return numbers;
    }
  }

  uint32_t PaletteColor(uint8_t index)
  {
    if (index < 16) return _campbell[index];

    //A 6x6x6 cube, then 24 grays
    if (index < 232)
    {
      static const uint8_t levels[] = { 0, 95, 135, 175, 215, 255 };
      auto cube = index - 16;
      return (uint32_t(levels[cube / 36]) << 16) | (uint32_t(levels[cube / 6 % 6]) << 8) | levels[cube % 6];
    }

    auto gray = uint32_t(8 + 10 * (index - 232));
    return (gray << 16) | (gray << 8) | gray;
  }

  AnsiParser::AnsiParser(TextStyle style) :
    _style(style)
  { }

  const TextStyle& AnsiParser::Style() const
  {
    return _style;
  }

  vector<StyledSpan> AnsiParser::Parse(string_view text)
  {
    vector<StyledSpan> spans;
    auto add = [&](string_view part) {
      if (part.empty()) return;
      if (!spans.empty() && spans.back().Style == _style) spans.back().Text += part;
      else spans.push_back({ string(part), _style });
      };

    while (!text.empty())
    {
      auto escape = text.find(_escape);
      add(text.substr(0, escape));
      if (escape == string_view::npos) break;

      text.remove_prefix(escape);
      string_view parameters;
      char final;
      auto length = max<size_t>(CodeLength(text, parameters, final), 1);
      if (final == 'm') Apply(parameters);
      text.remove_prefix(length);
    }
    return spans;
  }

  void AnsiParser::Apply(string_view parameters)
  {
    auto numbers = Numbers(parameters);
    for (size_t index = 0; index < numbers.size(); index++)
    {
      auto code = numbers[index];

      //An extended color: 5;n from the palette, or 2;r;g;b
      auto extended = [&]() -> optional<uint32_t> {
        if (index + 2 < numbers.size() && numbers[index + 1] == 5)
        {
          index += 2;
          return PaletteColor(uint8_t(clamp(numbers[index], 0, 255)));
        }
        if (index + 4 < numbers.size() && numbers[index + 1] == 2)
        {
          auto red = clamp(numbers[index + 2], 0, 255), green = clamp(numbers[index + 3], 0, 255), blue = clamp(numbers[index + 4], 0, 255);
          index += 4;
          return (uint32_t(red) << 16) | (uint32_t(green) << 8) | uint32_t(blue);
        }
        index = numbers.size();
        return nullopt;
        };

      switch (code)
      {
      case 0: _style = {}; break;
      case 1: _style.IsBold = true; break;
      case 2: _style.IsDim = true; break;
      case 3: _style.IsItalic = true; break;
      case 4: _style.IsUnderlined = true; break;
      case 7: _style.IsInverse = true; break;
      case 9: _style.IsStruck = true; break;
      case 21: _style.IsUnderlined = true; break;
      case 22: _style.IsBold = false; _style.IsDim = false; break;
      case 23: _style.IsItalic = false; break;
      case 24: _style.IsUnderlined = false; break;
      case 27: _style.IsInverse = false; break;
      case 29: _style.IsStruck = false; break;
      case 38: _style.Foreground = extended(); break;
      case 39: _style.Foreground = nullopt; break;
      case 48: _style.Background = extended(); break;
      case 49: _style.Background = nullopt; break;
      default:
        if (code >= 30 && code <= 37) _style.Foreground = PaletteColor(uint8_t(code - 30));
        else if (code >= 40 && code <= 47) _style.Background = PaletteColor(uint8_t(code - 40));
        else if (code >= 90 && code <= 97) _style.Foreground = PaletteColor(uint8_t(code - 90 + 8));
        else if (code >= 100 && code <= 107) _style.Background = PaletteColor(uint8_t(code - 100 + 8));
        break;
      }
    }
  }

  string StripEscapes(string_view text)
  {
    if (text.find(_escape) == string_view::npos) return string(text);

    string result;
    result.reserve(text.size());
    while (!text.empty())
    {
      auto escape = text.find(_escape);
      result += text.substr(0, escape);
      if (escape == string_view::npos) break;

      text.remove_prefix(escape);
      string_view parameters;
      char final;
      text.remove_prefix(max<size_t>(CodeLength(text, parameters, final), 1));
    }
    return result;
  }
}
