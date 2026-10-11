#pragma once
#include "JsonRpcError.h"
#include "McpMessages.h"
#include "ProtocolErrors.h"
#include "StrictJson.h"
#include "ToolContext.h"

namespace SecondShell::Mcp
{
  struct ToolDefinition
  {
    std::string Name;
    std::string Description;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> InputSchema;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> OutputSchema;
    std::function<CallToolResponse(const Axodox::Json::json_object& arguments, ToolContext& context)> Call;
  };

  //A set of tools, registered in registry<ToolSource>. A derived source adds each tool from a member function. The
  //tool's description and schemas come from its argument and result types, which also parse its arguments, so what the
  //client is told and what the tool accepts cannot disagree.
  class ToolSource
  {
  public:
    virtual ~ToolSource() = default;

    std::span<const ToolDefinition> Tools() const;

  protected:
    //Adds a tool in the constructor of the derived source. Its result becomes the structured content of the response,
    //and its error a tool error.
    template<typename TSource, typename TArguments, typename TResult>
    void AddTool(const char* name, Rpc::RpcResult<TResult>(TSource::* tool)(const TArguments& arguments, ToolContext& context))
    {
      auto& description = TArguments::json_description;
      _tools.push_back({
        .Name = name,
        .Description = description.description ? description.description : "",
        .InputSchema = InputSchema(description.to_json()),
        .OutputSchema = TResult::json_description.to_json(),
        .Call = [source = static_cast<TSource*>(this), tool](const Axodox::Json::json_object& json, ToolContext& context) {
          TArguments arguments{};
          auto parsing = Rpc::StrictJson::Parse(json, arguments);
          if (!parsing) return CallToolResponse::Error(std::format("Invalid arguments: {}", parsing.error()));

          auto result = (source->*tool)(arguments, context);
          if (auto error = std::get_if<Rpc::JsonRpcError>(&result)) return CallToolResponse::Error(Protocol::ProtocolErrors::ToText(*error));

          return ResultResponse(Axodox::Json::json_serializer<TResult>::to_json(std::get<TResult>(result)), context);
        }
        });
    }

  private:
    std::vector<ToolDefinition> _tools;

    static Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> InputSchema(Axodox::Infrastructure::value_ptr<Axodox::Json::json_value>&& schema);
    static CallToolResponse ResultResponse(Axodox::Infrastructure::value_ptr<Axodox::Json::json_value>&& result, ToolContext& context);
  };
}
