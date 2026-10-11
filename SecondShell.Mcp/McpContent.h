#pragma once

namespace SecondShell::Mcp
{
  //An item of a tool result's content. Its type discriminator is "type".
  struct ContentBlock
  {
    static Axodox::Json::json_object_descriptor<ContentBlock> json_description;

    virtual ~ContentBlock() = default;
  };

  struct TextContent : public ContentBlock
  {
    static Axodox::Json::json_object_descriptor<TextContent> json_description;

    std::string Text;
  };

  struct ImageContent : public ContentBlock
  {
    static Axodox::Json::json_object_descriptor<ImageContent> json_description;

    std::vector<uint8_t> Data;
    std::string MimeType;
  };
}

namespace Axodox::Json
{
  template<>
  struct json_type_metadata<Axodox::Infrastructure::value_ptr<SecondShell::Mcp::ContentBlock>>
  {
    using type = json_any_schema;
  };
}
