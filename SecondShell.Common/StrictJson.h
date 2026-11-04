#pragma once
#include "CommonApi.h"

namespace SecondShell::Rpc
{
  //Parses described objects written by the AI: validates them, and rejects unknown properties, which are most likely
  //typos that would otherwise be silently ignored. Only the top level is checked for unknown properties. Every problem
  //is reported at once, so the AI can fix them in one go.
  struct StrictJson
  {
    template<typename TObject>
    static Axodox::Infrastructure::expected<> Parse(const Axodox::Json::json_object& json, TObject& result)
    {
      auto& description = TObject::json_description;
      std::vector<std::string> problems;

      auto hasUnknown = false;
      for (auto& [name, value] : json.value)
      {
        if (name == description.type_discriminator() || description.find_property(name)) continue;
        problems.push_back(std::format("unknown property '{}'", name));
        hasUnknown = true;
      }

      //As the object schema validates, but on to the end
      for (auto& property : description.properties())
      {
        Axodox::Json::json_value* item;
        if (!json.try_get_value(property.name(), item) || !item)
        {
          if (property.is_required() == true) problems.push_back(std::format("'{}' is required", property.name()));
          continue;
        }
        if (item->type() == Axodox::Json::json_type::null && property.is_required() != true) continue;

        auto validation = property.validate(*item);
        if (!validation) problems.push_back(std::format("'{}': {}", property.name(), validation.error()));
      }

      if (hasUnknown) problems.push_back(std::format("the properties are: {}", PropertyNames<TObject>()));
      if (!problems.empty()) return Axodox::Infrastructure::failure<>(Join(problems));

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

    //"a; b; c." from problems which may end with a period of their own
    static std::string Join(const std::vector<std::string>& problems)
    {
      std::string result;
      for (auto& problem : problems)
      {
        if (!result.empty()) result += "; ";
        result += std::string_view(problem).substr(0, problem.ends_with('.') ? problem.size() - 1 : problem.size());
      }
      return result + ".";
    }
  };
}
