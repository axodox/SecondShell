#include "pch.h"
#include "ActionLog.h"
#include "KeyboardActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Actions;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  const size_t _maxEntries = 1000;
  const size_t _maxSummaryLength = 200;

  namespace
  {
    value_ptr<json_value> ToJson(const value_ptr<Action>& action)
    {
      auto typing = dynamic_cast<const KeyboardTypeAction*>(action.get());
      if (!typing || !typing->Sensitive.value_or(false)) return json_serializer<value_ptr<Action>>::to_json(action);

      auto redacted = make_value<KeyboardTypeAction>(*typing);
      redacted->Text = string(typing->Text.size(), '*');
      return json_serializer<value_ptr<Action>>::to_json(value_ptr<Action>(move(redacted)));
    }

    //The name, then the arguments by name, e.g. "Mouse.Click button: \"Right\", x: 10, y: 20"
    string Summarize(const Action& action, const json_value& json)
    {
      auto& properties = json.as<json_object>().value;
      vector<string> names;
      for (auto& [name, value] : properties) if (name != "action") names.push_back(name);
      ranges::sort(names);

      string summary = action.Name();
      for (auto& name : names) summary += format("{} {}: {}", &name == &names.front() ? "" : ",", name, properties.at(name)->to_string());
      if (summary.size() <= _maxSummaryLength) return summary;

      //Not within a character
      auto length = _maxSummaryLength;
      while (length > 0 && (uint8_t(summary[length]) & 0xC0) == 0x80) length--;
      return summary.substr(0, length) + "...";
    }
  }

  ActionLog::ActionLog() :
    ActionStarted(_events),
    EntryAdded(_events)
  { }

  void ActionLog::Start(const value_ptr<Action>& action)
  {
    _events.raise(ActionStarted, this, Summarize(*action, *ToJson(action)));
  }

  void ActionLog::Add(const value_ptr<Action>& action, const ActionResult& result)
  {
    auto json = ToJson(action);
    ActionLogEntry entry{ system_clock::now(), json->to_string(), Summarize(*action, *json), result.Status, result.Message.value_or("") };
    _logger.log(result.Status == ActionStatus::Succeeded ? log_severity::information : log_severity::warning, "{} {} {}", entry.Action, to_string(entry.Status), entry.Message);

    {
      lock_guard lock{ _mutex };
      _entries.push_back(entry);
      if (_entries.size() > _maxEntries) _entries.pop_front();
    }

    _events.raise(EntryAdded, this, entry);
  }

  vector<ActionLogEntry> ActionLog::Entries() const
  {
    lock_guard lock{ _mutex };
    return { _entries.begin(), _entries.end() };
  }
}
