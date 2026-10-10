#pragma once
#include "Action.h"
#include "ActionResult.h"

namespace SecondShell::Actions
{
  //One action type of the catalog.
  struct ActionDefinition
  {
    const char* Name;
    std::function<Axodox::Infrastructure::value_ptr<Axodox::Json::json_value>()> Schema;
    std::function<Axodox::Infrastructure::expected<Axodox::Infrastructure::value_ptr<Action>>(const Axodox::Json::json_object& json)> Parse;
  };

  //Every action of Run-Sequence. Parses and validates actions by their "action" name with the action's own type, and
  //describes them all as one JSON schema (any of the actions), which is what the AI client sees for a list of steps.
  //The base descriptor cannot do this: it validates and describes only the properties of the static type.
  class SECONDSHELL_COMMON_API ActionCatalog
  {
  public:
    static std::span<const ActionDefinition> Actions();

    //Fails with a message for the AI: unknown action, unknown or invalid property, missing required property.
    static Axodox::Infrastructure::expected<Axodox::Infrastructure::value_ptr<Action>> Parse(const Axodox::Json::json_value& json);

    static Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Schema();
    static Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> ResultSchema();
  };
}
