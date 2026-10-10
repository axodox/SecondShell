#pragma once
#include "CommonApi.h"

namespace SecondShell::Actions
{
  //A window of the child session: its handle, written as a hexadecimal string such as "0x0001A2B4".
  //Valid only inside the child session and only while the window exists.
  struct SECONDSHELL_COMMON_API WindowId
  {
    uint64_t Handle = 0;

    std::string to_string() const;
    static std::optional<WindowId> from_string(std::string_view text);

    bool operator==(const WindowId& other) const = default;
  };
}

namespace Axodox::Json
{
  template<>
  struct json_type_metadata<SecondShell::Actions::WindowId>
  {
    using type = json_string_schema;
  };
}
