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
    //From Build\SecondShell.Cpp.props
    return SECONDSHELL_VERSION;
  }
}