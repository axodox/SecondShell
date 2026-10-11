#include "pch.h"
#include "McpContent.h"

using namespace Axodox::Json;

namespace SecondShell::Mcp
{
  json_object_descriptor<ContentBlock> ContentBlock::json_description = describe_json_object<ContentBlock>({ .type_discriminator = "type" }, {});

  json_object_descriptor<TextContent> TextContent::json_description = describe_json_object<TextContent, ContentBlock>(
    { .name = "text" },
    {
      { &TextContent::Text, "text" }
    });

  json_object_descriptor<ImageContent> ImageContent::json_description = describe_json_object<ImageContent, ContentBlock>(
    { .name = "image" },
    {
      { &ImageContent::Data, "data", json_base64_converter{} },
      { &ImageContent::MimeType, "mimeType" }
    });
}
