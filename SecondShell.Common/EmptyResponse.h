#pragma once
#include "CommonApi.h"

namespace SecondShell::Protocol
{
  //The response of requests which only report success or an error.
  struct SECONDSHELL_COMMON_API EmptyResponse
  {
    static Axodox::Json::json_object_descriptor<EmptyResponse> json_description;
  };
}
