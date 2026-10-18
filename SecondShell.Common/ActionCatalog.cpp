#include "pch.h"
#include "ActionCatalog.h"
#include "CaptureActions.h"
#include "ClipboardActions.h"
#include "ControlActions.h"
#include "KeyboardActions.h"
#include "MouseActions.h"
#include "ProcessActions.h"
#include "StrictJson.h"
#include "PenActions.h"
#include "TouchActions.h"
#include "WindowActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Actions
{
  namespace
  {
    //The schema of one type of a hierarchy: its own schema, with the discriminator fixed to its name, and nothing else allowed.
    template<typename TObject>
    value_ptr<json_value> DescribeDerived(const char* discriminator)
    {
      auto& description = TObject::json_description;
      auto schema = description.to_json();
      auto& object = schema->template as<json_object>();

      auto discriminatorSchema = make_value<json_object>();
      discriminatorSchema->set_value("type", "string");
      discriminatorSchema->set_value("const", description.name());
      object.at("properties")->template as<json_object>().set_value(discriminator, value_ptr<json_value>(move(discriminatorSchema)));

      auto required = make_value<json_array>();
      required->value.push_back(make_value<json_string>(discriminator));
      if (object.value.contains("required"))
      {
        for (auto& item : object.at("required")->template as<json_array>().value) required->value.push_back(item);
      }
      object.set_value("required", value_ptr<json_value>(move(required)));
      object.set_value("additionalProperties", false);

      return schema;
    }

    template<typename TAction>
    expected<value_ptr<Action>> ParseAction(const json_object& json)
    {
      auto action = make_value<TAction>();
      auto parsing = StrictJson::Parse(json, *action);
      if (!parsing) return format_unexpected("{}: {}", TAction::json_description.name(), parsing.error());

      return value_ptr<Action>(move(action));
    }

    template<typename TAction>
    ActionDefinition Define()
    {
      return {
        .Name = TAction::json_description.name(),
        .Schema = [] { return DescribeDerived<TAction>(Action::json_description.type_discriminator()); },
        .Parse = &ParseAction<TAction>
      };
    }

    const vector<ActionDefinition>& Definitions()
    {
      static const vector<ActionDefinition> result = {
        Define<ControlWaitAction>(),
        Define<ControlJoinAction>(),
        Define<ControlResetAction>(),
        Define<MouseMoveAction>(),
        Define<MouseClickAction>(),
        Define<MouseDoubleClickAction>(),
        Define<MouseWheelAction>(),
        Define<MousePressAction>(),
        Define<MouseReleaseAction>(),
        Define<MouseResetAction>(),
        Define<TouchPressAction>(),
        Define<TouchReleaseAction>(),
        Define<TouchTapAction>(),
        Define<TouchMoveAction>(),
        Define<TouchResetAction>(),
        Define<PenMoveAction>(),
        Define<PenPressAction>(),
        Define<PenReleaseAction>(),
        Define<PenTapAction>(),
        Define<PenResetAction>(),
        Define<KeyboardTypeAction>(),
        Define<KeyboardTapAction>(),
        Define<KeyboardPressAction>(),
        Define<KeyboardReleaseAction>(),
        Define<KeyboardResetAction>(),
        Define<CaptureScreenAction>(),
        Define<CaptureWindowAction>(),
        Define<CaptureRegionAction>(),
        Define<CaptureWaitForTextAction>(),
        Define<CaptureWaitForStableAction>(),
        Define<WindowEnumerateAction>(),
        Define<WindowFocusAction>(),
        Define<WindowMoveAction>(),
        Define<WindowResizeAction>(),
        Define<WindowMinimizeAction>(),
        Define<WindowMaximizeAction>(),
        Define<WindowRestoreAction>(),
        Define<WindowCloseAction>(),
        Define<ProcessRunAction>(),
        Define<ProcessStopAction>(),
        Define<ProcessKillAction>(),
        Define<ProcessWaitForProcessAction>(),
        Define<ProcessEnumerateAction>(),
        Define<ClipboardReadAction>(),
        Define<ClipboardWriteAction>()
      };
      return result;
    }

    //The actions of the group the unknown name refers to (e.g. "Mouse.Scroll"), or all of them.
    string SuggestActions(string_view name)
    {
      auto group = to_lower(name.substr(0, name.find('.') + 1));
      string matching, all;
      for (auto& definition : Definitions())
      {
        string_view actionName = definition.Name;
        all += all.empty() ? "" : ", ";
        all += actionName;

        if (!group.empty() && to_lower(actionName).starts_with(group))
        {
          matching += matching.empty() ? "" : ", ";
          matching += actionName;
        }
      }
      return matching.empty() ? all : matching;
    }

    value_ptr<json_value> AnyOf(vector<value_ptr<json_value>>&& schemas)
    {
      auto items = make_value<json_array>();
      items->value = move(schemas);

      auto result = make_value<json_object>();
      result->set_value("anyOf", value_ptr<json_value>(move(items)));
      return result;
    }
  }

  span<const ActionDefinition> ActionCatalog::Actions()
  {
    return Definitions();
  }

  expected<value_ptr<Action>> ActionCatalog::Parse(const json_value& json)
  {
    auto object = json.try_as<json_object>();
    if (!object) return failure<>("An action must be an object, e.g. {\"action\": \"Mouse.Click\", \"x\": 100, \"y\": 200}.");

    json_value* nameValue;
    auto name = object->try_get_value("action", nameValue) && nameValue ? nameValue->try_as<json_string>() : nullptr;
    if (!name) return failure<>("An action needs its name in the \"action\" property, e.g. \"Mouse.Click\".");

    //Names are matched case-insensitively, as enum values are
    auto key = to_lower(name->value);
    for (auto& definition : Definitions())
    {
      if (to_lower(definition.Name) == key) return definition.Parse(*object);
    }

    return format_unexpected("Unknown action '{}'. Actions: {}.", name->value, SuggestActions(name->value));
  }

  value_ptr<json_value> ActionCatalog::Schema()
  {
    static const auto result = [] {
      vector<value_ptr<json_value>> schemas;
      for (auto& definition : Definitions()) schemas.push_back(definition.Schema());
      return AnyOf(move(schemas));
      }();
    return result;
  }

  value_ptr<json_value> ActionCatalog::ResultSchema()
  {
    static const auto result = [] {
      auto discriminator = ActionResult::json_description.type_discriminator();
      return AnyOf({
        DescribeDerived<ActionResult>(discriminator),
        DescribeDerived<CaptureResult>(discriminator),
        DescribeDerived<ProcessResult>(discriminator),
        DescribeDerived<ProcessListResult>(discriminator),
        DescribeDerived<WindowListResult>(discriminator),
        DescribeDerived<ClipboardResult>(discriminator)
        });
      }();
    return result;
  }

  value_ptr<json_value> ActionSchema::to_json() const
  {
    return ActionCatalog::Schema();
  }

  expected<> ActionSchema::validate_value(const json_value& value) const
  {
    auto result = ActionCatalog::Parse(value);
    if (!result) return failure<>(result.error());
    return {};
  }

  value_ptr<json_value> ActionResultSchema::to_json() const
  {
    return ActionCatalog::ResultSchema();
  }
}
