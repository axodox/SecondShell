#pragma once
#include "CommonApi.h"

namespace SecondShell::Actions
{
  //Key names: the members of the .NET System.Windows.Forms.Keys enumeration, whose values are the Windows virtual key
  //codes, plus a few aliases models tend to use (Ctrl, Alt, Shift, Win, Esc, Backspace, 0-9).
  class SECONDSHELL_COMMON_API KeyNames
  {
  public:
    //A name or alias, case-insensitive.
    static std::optional<uint16_t> TryParse(std::string_view name);

    //The canonical name of a virtual key code, or the code as a number if it has no name.
    static std::string ToString(uint16_t key);
  };

  //Schema of a key: a key name, or a virtual key code as a number.
  struct SECONDSHELL_COMMON_API KeySchema : public Axodox::Json::json_type_schema<KeySchema, Axodox::Json::json_type::null>
  {
    const char* description = nullptr;

    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> to_json() const;
    Axodox::Infrastructure::expected<> validate_value(const Axodox::Json::json_value& value) const;
  };

  //Converts a virtual key code to its name, and a name or a number to a virtual key code.
  struct SECONDSHELL_COMMON_API KeyConverter
  {
    using json_schema_type = KeySchema;

    static Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> to_json(uint16_t key);
    static bool from_json(const Axodox::Json::json_value* json, uint16_t& key);
    static bool is_default(uint16_t key);
  };
}
