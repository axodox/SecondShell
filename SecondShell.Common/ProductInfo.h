#pragma once
#include "CommonApi.h"

namespace SecondShell::Environment
{
  struct SECONDSHELL_COMMON_API ProductInfo
  {
    static const char* Name();
    static const char* Version();
  };
}