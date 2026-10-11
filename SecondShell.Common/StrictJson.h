#pragma once
#include "CommonApi.h"

namespace SecondShell::Rpc
{
  //Parses described objects written by the AI: validates them, and rejects unknown properties, which are most likely
  //typos that would otherwise be silently ignored. Only the top level is checked for unknown properties.
  struct StrictJson
  {
    template<typename TObject>
    static Axodox::Infrastructure::expected<> Parse(const Axodox::Json::json_object& json, TObject& result)
    {
      auto& description = TObject::json_description;
      for (auto& [name, value] : json.value)
      {
        if (name == description.type_discriminator()) continue;
        if (!description.find_property(name)) return Axodox::Infrastructure::format_unexpected("unknown property '{}'; the properties are: {}.", name, PropertyNames<TObject>());
      }

      auto validation = description.validate(json);
      if (!validation) return validation;

      return description.from_json(result, &json);
    }

  private:
    template<typename TObject>
    static std::string PropertyNames()
    {
      std::string result;
      for (auto& property : TObject::json_description.properties())
      {
        if (!result.empty()) result += ", ";
        result += property.name();
      }
      return result.empty() ? "none" : result;
    }
  };
}
