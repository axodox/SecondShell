#include "pch.h"
#include "ActionLog.h"
#include "ClipboardActions.h"
#include "KeyboardActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  const size_t _maxSequences = 200;
  const size_t _maxSummaryLength = 200;
  const size_t _maxValueLength = 60;
  const size_t _maxOutputLength = 8192;

  //In UTF-8, whatever the compiler's execution character set
  const char* const _dot = "\xE2\x80\xA2";
  const char* const _ellipsis = "\xE2\x80\xA6";
  const char* const _times = "\xC3\x97";

  namespace
  {
    //Not within a character
    string Shorten(string text, size_t limit, string_view ellipsis)
    {
      if (text.size() <= limit) return text;

      auto length = limit;
      while (length > 0 && (uint8_t(text[length]) & 0xC0) == 0x80) length--;
      return text.substr(0, length) + string(ellipsis);
    }

    value_ptr<json_value> ToJson(const value_ptr<Action>& action)
    {
      auto typing = dynamic_cast<const KeyboardTypeAction*>(action.get());
      if (!typing || !typing->Sensitive.value_or(false)) return json_serializer<value_ptr<Action>>::to_json(action);

      auto redacted = make_value<KeyboardTypeAction>(*typing);
      redacted->Text = string(typing->Text.size(), '*');
      return json_serializer<value_ptr<Action>>::to_json(value_ptr<Action>(move(redacted)));
    }

    vector<string> SortedNames(const json_object& object)
    {
      vector<string> names;
      for (auto& [name, value] : object.value) if (name != "action") names.push_back(name);
      ranges::sort(names);
      return names;
    }

    //The name, then the arguments by name, e.g. "Mouse.Click button: \"Right\", x: 10, y: 20"
    string Summarize(const Action& action, const json_value& json)
    {
      auto& object = json.as<json_object>();
      auto names = SortedNames(object);

      string summary = action.Name();
      for (auto& name : names) summary += format("{} {}: {}", &name == &names.front() ? "" : ",", name, object.value.at(name)->to_string());
      return Shorten(move(summary), _maxSummaryLength, "...");
    }

    string FormatMilliseconds(double milliseconds)
    {
      if (milliseconds < 1000) return format("{:.0f} ms", milliseconds);

      auto seconds = milliseconds / 1000;
      if (seconds < 60) return seconds == floor(seconds) || seconds >= 10 ? format("{:.0f} s", seconds) : format("{:.1f} s", seconds);

      auto minutes = int(seconds / 60);
      auto rest = int(seconds) % 60;
      return rest ? format("{} min {} s", minutes, rest) : format("{} min", minutes);
    }

    string KeyCap(string_view key)
    {
      static const pair<string_view, string_view> names[] = {
        { "Escape", "Esc" }, { "ControlKey", "Ctrl" }, { "LControlKey", "Ctrl" }, { "RControlKey", "Ctrl" },
        { "ShiftKey", "Shift" }, { "LShiftKey", "Shift" }, { "RShiftKey", "Shift" }, { "Menu", "Alt" },
        { "LMenu", "Alt" }, { "RMenu", "Alt" }, { "LWin", "Win" }, { "RWin", "Win" }, { "Return", "Enter" },
        { "Back", "Backspace" }, { "Next", "PageDown" }, { "Prior", "PageUp" }
      };
      for (auto& [name, cap] : names) if (name == key) return string(cap);
      return string(key);
    }

    //A value as the user reads it; durations are in milliseconds in every action
    ActionArgument ToArgument(const string& name, const json_value& value)
    {
      auto isDuration = name == "duration" || name == "timeout" || name == "interval";
      switch (value.type())
      {
      case json_type::string:
        return { name, "\"" + Shorten(value.as<json_string>().value, _maxValueLength, _ellipsis) + "\"", true };
      case json_type::number:
        return { name, isDuration ? FormatMilliseconds(value.as<json_number>().value) : value.to_string() };
      case json_type::object:
      {
        auto& object = value.as<json_object>().value;
        if (object.contains("width") && object.contains("height") && object.size() == 2)
        {
          return { name, format("{} {} {}", object.at("width")->to_string(), _times, object.at("height")->to_string()) };
        }
        break;
      }
      default:
        break;
      }
      return { name, Shorten(value.to_string(), _maxValueLength, _ellipsis) };
    }

    ActionPresentation Present(const Action& action, const json_value& json)
    {
      auto& object = json.as<json_object>();
      ActionPresentation presentation{ .Name = action.Name() };

      auto typing = dynamic_cast<const KeyboardTypeAction*>(&action);
      auto isSensitive = typing && typing->Sensitive.value_or(false);
      for (auto& name : SortedNames(object))
      {
        auto& value = *object.value.at(name);
        if (name == "key" && value.type() == json_type::string) continue;
        if (name == "modifiers" && value.type() == json_type::array)
        {
          for (auto& modifier : value.as<json_array>().value) presentation.Keys.push_back(modifier->as<json_string>().value);
          continue;
        }
        if (isSensitive && name == "sensitive") continue;
        if (isSensitive && name == "text")
        {
          string dots;
          for (size_t index = 0; index < min<size_t>(typing->Text.size(), 8); index++) dots += _dot;
          presentation.Arguments.push_back({ name, "\"" + dots + "\"", true });
          continue;
        }

        presentation.Arguments.push_back(ToArgument(name, value));
      }

      if (object.value.contains("key"))
      {
        if (auto& key = *object.value.at("key"); key.type() == json_type::string) presentation.Keys.push_back(KeyCap(key.as<json_string>().value));
      }
      return presentation;
    }

    optional<CaptureLogInfo> CaptureOf(const ActionResult& result, const optional<filesystem::path>& captureFile)
    {
      const CapturedImage* image = nullptr;
      if (auto capture = dynamic_cast<const CaptureResult*>(&result)) image = &capture->Image;
      else if (auto clipboard = dynamic_cast<const ClipboardResult*>(&result); clipboard && clipboard->Image) image = &*clipboard->Image;
      if (!image || !image->Width) return nullopt;

      auto file = image->Path ? *image->Path : captureFile ? winrt::to_string(captureFile->wstring()) : string();
      return CaptureLogInfo{ image->Width, image->Height, image->Format, image->Data.size(), !image->Data.empty(), file };
    }

    optional<ProcessLogInfo> ProcessOf(const ActionResult& result)
    {
      auto process = dynamic_cast<const ProcessResult*>(&result);
      if (!process) return nullopt;

      auto output = process->StandardOutput.value_or("");
      if (process->StandardError && !process->StandardError->empty()) output += (output.empty() ? "" : "\n") + *process->StandardError;
      return ProcessLogInfo{ process->ProcessId, process->ExitCode, Shorten(move(output), _maxOutputLength, string("\n") + _ellipsis) };
    }
  }

  string FormatDuration(milliseconds duration)
  {
    return FormatMilliseconds(double(duration.count()));
  }

  ActionLog::ActionLog() :
    SequenceChanged(_events),
    ActionStarted(_events),
    EntryAdded(_events)
  { }

  void ActionLog::BeginSequence(uint32_t number, const optional<string>& description, uint32_t stepCount)
  {
    SequenceLogEntry sequence{ number, description.value_or(""), stepCount, system_clock::now() };
    {
      lock_guard lock{ _mutex };
      _sequences.push_back(sequence);
      if (_sequences.size() > _maxSequences) _sequences.pop_front();
    }
    _events.raise(SequenceChanged, this, sequence);
  }

  void ActionLog::Start(const value_ptr<Action>& action, uint32_t step)
  {
    auto entry = Describe(action, step);
    {
      lock_guard lock{ _mutex };
      _running = entry;
    }
    _events.raise(ActionStarted, this, entry);
  }

  void ActionLog::Add(const value_ptr<Action>& action, const ActionResult& result, const optional<filesystem::path>& captureFile)
  {
    optional<ActionLogEntry> running;
    {
      lock_guard lock{ _mutex };
      running = exchange(_running, nullopt);
    }

    auto entry = running ? move(*running) : Describe(action, 0);

    entry.Duration = duration_cast<milliseconds>(system_clock::now() - entry.Time);
    entry.Status = result.Status;
    entry.Message = result.Message.value_or("");
    entry.Capture = CaptureOf(result, captureFile);
    entry.Process = ProcessOf(result);
    _logger.log(result.Status == ActionStatus::Succeeded ? log_severity::information : log_severity::warning, "{} {} {}", entry.Action, to_string(result.Status), entry.Message);

    {
      lock_guard lock{ _mutex };
      Current().Steps.push_back(entry);
    }
    _events.raise(EntryAdded, this, entry);
  }

  void ActionLog::EndSequence(const RunSequenceResponse& response)
  {
    SequenceLogEntry sequence;
    {
      lock_guard lock{ _mutex };
      auto& current = Current();
      current.Duration = duration_cast<milliseconds>(system_clock::now() - current.Time);
      current.Error = response.Error;
      sequence = current;
      sequence.Steps.clear();
    }
    _events.raise(SequenceChanged, this, sequence);
  }

  vector<SequenceLogEntry> ActionLog::Sequences() const
  {
    lock_guard lock{ _mutex };
    vector<SequenceLogEntry> sequences{ _sequences.begin(), _sequences.end() };
    if (_running && !sequences.empty()) sequences.back().Steps.push_back(*_running);
    return sequences;
  }

  ActionLogEntry ActionLog::Describe(const value_ptr<Action>& action, uint32_t step) const
  {
    auto json = ToJson(action);

    ActionLogEntry entry;
    entry.Step = step;
    entry.Time = system_clock::now();
    entry.Action = json->to_string();
    entry.Summary = Summarize(*action, *json);
    entry.Presentation = Present(*action, *json);

    lock_guard lock{ _mutex };
    if (!_sequences.empty()) entry.Sequence = _sequences.back().Number;
    return entry;
  }

  SequenceLogEntry& ActionLog::Current()
  {
    //Actions logged outside a sequence get one of their own
    if (_sequences.empty() || _sequences.back().Duration) _sequences.push_back({ .Number = 0, .Time = system_clock::now() });
    return _sequences.back();
  }
}
