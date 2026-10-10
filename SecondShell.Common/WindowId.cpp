#include "pch.h"
#include "WindowId.h"

using namespace std;

namespace SecondShell::Actions
{
  string WindowId::to_string() const
  {
    return format("0x{:08X}", Handle);
  }

  optional<WindowId> WindowId::from_string(string_view text)
  {
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    if (text.empty()) return nullopt;

    WindowId result;
    auto [end, error] = from_chars(text.data(), text.data() + text.size(), result.Handle, 16);
    if (error != errc{} || end != text.data() + text.size()) return nullopt;

    return result;
  }
}
