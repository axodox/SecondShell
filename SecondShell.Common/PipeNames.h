#pragma once
#include "CommonApi.h"

namespace SecondShell::Environment
{
  //Names of the pipes shared by the processes. They contain the user's SID, so the processes of different users
  //never meet; the pipes are also restricted to the current user.
  struct SECONDSHELL_COMMON_API PipeNames
  {
    static std::string Driver();
    static std::string Mcp();
  };
}
