#pragma once
#include "ActionEnums.h"

namespace SecondShell::Actions
{
  //Base of all actions of Run-Sequence. Its type discriminator is "action", and each action's descriptor name is the
  //action name, e.g. "Mouse.Click". Parse and validate actions with ActionCatalog, which knows every action type.
  struct SECONDSHELL_COMMON_API Action
  {
    static Axodox::Json::json_object_descriptor<Action> json_description;

    virtual ~Action() = default;

    virtual ActionKind Kind() const;
    virtual ActionType Type() const;

    //Async actions run in parallel with the following ones, until Control.Join.
    virtual bool IsAsync() const;

    //For actions which wait for something: how long they may wait, if the action sets it.
    virtual std::optional<std::chrono::milliseconds> WaitTimeout() const;

    //The action name, e.g. "Mouse.Click".
    const char* Name() const;
  };

  //Implements Kind and Type for a concrete action.
  template<ActionKind kind, ActionType type, typename TBase = Action>
  struct TypedAction : public TBase
  {
    virtual ActionKind Kind() const override
    {
      return kind;
    }

    virtual ActionType Type() const override
    {
      return type;
    }
  };

  //The schema of an action property: any of the actions, with their own properties (see ActionCatalog).
  struct SECONDSHELL_COMMON_API ActionSchema : public Axodox::Json::json_type_schema<ActionSchema, Axodox::Json::json_type::object>
  {
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> to_json() const;
    Axodox::Infrastructure::expected<> validate_value(const Axodox::Json::json_value& value) const;
  };
}

namespace Axodox::Json
{
  template<>
  struct json_type_metadata<Axodox::Infrastructure::value_ptr<SecondShell::Actions::Action>>
  {
    using type = SecondShell::Actions::ActionSchema;
  };
}
