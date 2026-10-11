#include "pch.h"
#include "JsonSnapshot.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Tests
{
  string JsonSnapshot::ToCanonicalString(const json_value& json)
  {
    string result;
    Write(result, json, 0);
    result += "\n";
    return result;
  }

  void JsonSnapshot::AssertMatches(string_view name, const json_value& json)
  {
    auto file = path(__FILE__).parent_path() / "Snapshots" / (string(name) + ".json");
    auto actual = ToCanonicalString(json);

    char* update = nullptr;
    size_t length = 0;
    _dupenv_s(&update, &length, "SECONDSHELL_UPDATE_SNAPSHOTS");
    auto isUpdating = update && string_view(update) == "1";
    free(update);

    if (isUpdating || !exists(file))
    {
      create_directories(file.parent_path());
      ofstream(file, ios::binary) << actual;
      if (!isUpdating) Assert::Fail(format(L"Snapshot {} was created; review it and run the test again.", file.wstring()).c_str());
      return;
    }

    stringstream expected;
    expected << ifstream(file, ios::binary).rdbuf();
    if (expected.str() != actual)
    {
      ofstream(path(file).replace_extension(".actual.json"), ios::binary) << actual;
      Assert::Fail(format(L"{} does not match its snapshot; the new output is next to it as .actual.json.", file.filename().wstring()).c_str());
    }
  }

  void JsonSnapshot::AssertEqual(string_view expected, const json_value& actual)
  {
    auto expectedJson = json_value::from_string(expected);
    Assert::IsTrue(bool(expectedJson), L"the expected JSON should be valid");
    Assert::AreEqual(ToCanonicalString(*expectedJson), ToCanonicalString(actual));
  }

  void JsonSnapshot::Write(string& text, const json_value& json, size_t depth)
  {
    auto indent = [&](size_t level) { text.append(level * 2, ' '); };

    switch (json.type())
    {
    case json_type::object:
    {
      auto& items = json.as<json_object>().value;
      map<string, const json_value*> sorted;
      for (auto& [key, value] : items) sorted[key] = value.get();

      if (sorted.empty())
      {
        text += "{}";
        break;
      }

      text += "{\n";
      size_t index = 0;
      for (auto& [key, value] : sorted)
      {
        indent(depth + 1);
        text += format("\"{}\": ", json_escape_string(key));
        if (value) Write(text, *value, depth + 1);
        else text += "null";
        text += ++index < sorted.size() ? ",\n" : "\n";
      }
      indent(depth);
      text += "}";
      break;
    }
    case json_type::array:
    {
      auto& items = json.as<json_array>().value;
      if (items.empty())
      {
        text += "[]";
        break;
      }

      text += "[\n";
      for (size_t index = 0; index < items.size(); index++)
      {
        indent(depth + 1);
        if (items[index]) Write(text, *items[index], depth + 1);
        else text += "null";
        text += index + 1 < items.size() ? ",\n" : "\n";
      }
      indent(depth);
      text += "]";
      break;
    }
    default:
      text += json.to_string();
      break;
    }
  }
}
