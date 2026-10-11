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

  namespace
  {
    string Describe(const value_ptr<Action>& action)
    {
      auto typing = dynamic_cast<const KeyboardTypeAction*>(action.get());
      if (!typing || !typing->Sensitive.value_or(false)) return json_serializer<value_ptr<Action>>::to_json(action)->to_string();

      auto redacted = make_value<KeyboardTypeAction>(*typing);
      redacted->Text = string(typing->Text.size(), '*');
      return json_serializer<value_ptr<Action>>::to_json(value_ptr<Action>(move(redacted)))->to_string();
    }
  }

  ActionLog::ActionLog() :
    EntryAdded(_events)
  { }

  void ActionLog::Add(const value_ptr<Action>& action, const ActionResult& result)
  {
    ActionLogEntry entry{ system_clock::now(), Describe(action), result.Status, result.Message.value_or("") };
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
