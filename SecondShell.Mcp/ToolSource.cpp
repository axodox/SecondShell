#include "pch.h"
#include "ToolSource.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace std;

namespace SecondShell::Mcp
{
  span<const ToolDefinition> ToolSource::Tools() const
  {
    return _tools;
  }

  value_ptr<json_value> ToolSource::InputSchema(value_ptr<json_value>&& schema)
  {
    //The description is the tool's, which the tool definition already carries; unknown arguments are rejected
    auto& object = schema->as<json_object>();
    object.value.erase("description");
    object.set_value("additionalProperties", false);
    return move(schema);
  }

  CallToolResponse ToolSource::ResultResponse(value_ptr<json_value>&& result, ToolContext& context)
  {
    //Clients which do not read structured content get it as text, as MCP recommends
    auto text = make_value<TextContent>();
    text->Text = result->to_string();

    CallToolResponse response;
    response.Content.push_back(move(text));
    for (auto& content : context.Content) response.Content.push_back(move(content));
    response.StructuredContent = move(result);
    return response;
  }
}
