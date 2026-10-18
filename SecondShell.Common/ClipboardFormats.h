#pragma once
#include "CommonApi.h"

namespace SecondShell::Desktop
{
  //Windows clipboard formats by name. Registered formats have numbers of their own in each session, so formats cross
  //between sessions by name.
  class SECONDSHELL_COMMON_API ClipboardFormats
  {
  public:
    //Standard formats by their CF_ names, registered ones by name, any by number; nullopt for an unknown name.
    static std::optional<uint32_t> FromName(std::string_view name);
    static std::string Name(uint32_t format);

    //Formats whose data is not memory but a GDI handle, which cannot be read or written as bytes.
    static bool IsHandle(uint32_t format);
  };
}
