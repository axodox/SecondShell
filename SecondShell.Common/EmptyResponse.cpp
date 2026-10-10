#include "pch.h"
#include "EmptyResponse.h"

using namespace Axodox::Json;

namespace SecondShell::Protocol
{
  json_object_descriptor<EmptyResponse> EmptyResponse::json_description = describe_json_object<EmptyResponse>({});
}
