#include "pch.h"
#include "ProductInfo.h"

namespace SecondShell::Environment
{
  const char* ProductInfo::Name()
  {
    return "SecondShell";
  }

  const char* ProductInfo::Version()
  {
    return "0.1.0";
  }
}