#pragma once
#include "CommonApi.h"

//Terminal output with escape codes (ECMA-48): the colors and styles of its text, as Windows Terminal shows them, and
//the text without the codes.
namespace SecondShell::Text
{
  //Colors are 0xRRGGBB; none is the terminal's default.
  struct SECONDSHELL_COMMON_API TextStyle
  {
    std::optional<uint32_t> Foreground;
    std::optional<uint32_t> Background;
    bool IsBold = false;
    bool IsDim = false;
    bool IsItalic = false;
    bool IsUnderlined = false;
    bool IsInverse = false;
    bool IsStruck = false;

    bool operator==(const TextStyle&) const = default;
  };

  struct SECONDSHELL_COMMON_API StyledSpan
  {
    std::string Text;
    TextStyle Style;
  };

  //Reads text with escape codes: select graphic rendition (SGR) changes the style, every other code (cursor moves,
  //window titles, ...) is dropped. The style carries on from one call to the next, as from line to line in a terminal.
  class SECONDSHELL_COMMON_API AnsiParser
  {
  public:
    explicit AnsiParser(TextStyle style = {});

    std::vector<StyledSpan> Parse(std::string_view text);

    //After what was parsed.
    const TextStyle& Style() const;

  private:
    TextStyle _style;

    void Apply(std::string_view parameters);
  };

  //The text without escape codes.
  SECONDSHELL_COMMON_API std::string StripEscapes(std::string_view text);

  //The 256 colors of a terminal: the first 16 as Windows Terminal's default scheme (Campbell) has them.
  SECONDSHELL_COMMON_API uint32_t PaletteColor(uint8_t index);
}
