#include "pch.h"
#include "JsonConfigEditor.h"

using namespace std;

namespace SecondShell::Integrations
{
  namespace
  {
    bool IsLiteralEnd(char character)
    {
      return character == ',' || character == '}' || character == ']' || character == '/' || isspace(uint8_t(character));
    }
  }

  JsonConfigEditor::JsonConfigEditor(string text) :
    _text(move(text))
  { }

  optional<string> JsonConfigEditor::Entry(string_view section, string_view name) const
  {
    if (_text.find_first_not_of(" \t\r\n") == string::npos) return nullopt;

    auto sectionMember = FindMember(RootObject(), section);
    if (!sectionMember || _text[sectionMember->ValueStart] != '{') return nullopt;

    auto member = FindMember(sectionMember->ValueStart, name);
    if (!member) return nullopt;
    return _text.substr(member->ValueStart, member->ValueEnd - member->ValueStart);
  }

  void JsonConfigEditor::SetEntry(string_view section, string_view name, const JsonEntry& entry)
  {
    //A new file: an object of its own, ending with a line break as editors write them
    if (_text.find_first_not_of(" \t\r\n") == string::npos) _text = format("{{}}{}", LineBreak());

    auto root = RootObject();
    auto sectionMember = FindMember(root, section);
    if (!sectionMember)
    {
      auto value = format("{{{0}{1}\"{2}\": {3}{0}{4}}}", LineBreak(), Indent(2), name, Render(entry, 2), Indent(1));
      AddMember(root, section, value, 0);
      return;
    }

    if (_text[sectionMember->ValueStart] != '{') throw runtime_error(format("\"{}\" in the file is not an object.", section));

    if (auto member = FindMember(sectionMember->ValueStart, name))
    {
      _text.replace(member->ValueStart, member->ValueEnd - member->ValueStart, Render(entry, 2));
      return;
    }

    AddMember(sectionMember->ValueStart, name, Render(entry, 2), 1);
  }

  void JsonConfigEditor::RemoveEntry(string_view section, string_view name)
  {
    if (_text.find_first_not_of(" \t\r\n") == string::npos) return;

    auto sectionMember = FindMember(RootObject(), section);
    if (!sectionMember || _text[sectionMember->ValueStart] != '{') return;

    auto members = Members(sectionMember->ValueStart);
    auto index = size_t(ranges::find_if(members, [&](const Member& member) { return StringAt(member.KeyStart) == name; }) - members.begin());
    if (index == members.size()) return;

    //With the comma between it and its neighbour; the last one alone leaves an empty object
    if (members.size() == 1)
    {
      auto closing = ValueEnd(sectionMember->ValueStart) - 1;
      _text.replace(sectionMember->ValueStart, closing + 1 - sectionMember->ValueStart, "{}");
    }
    else if (index + 1 < members.size())
    {
      _text.erase(members[index].KeyStart, members[index + 1].KeyStart - members[index].KeyStart);
    }
    else
    {
      _text.erase(members[index - 1].ValueEnd, members[index].ValueEnd - members[index - 1].ValueEnd);
    }
  }

  vector<string> JsonConfigEditor::EntryNames(string_view section) const
  {
    vector<string> names;
    if (_text.find_first_not_of(" \t\r\n") == string::npos) return names;

    auto sectionMember = FindMember(RootObject(), section);
    if (!sectionMember || _text[sectionMember->ValueStart] != '{') return names;

    for (auto& member : Members(sectionMember->ValueStart)) names.push_back(StringAt(member.KeyStart));
    return names;
  }

  void JsonConfigEditor::SetEntryProperty(string_view section, string_view name, string_view property, const string& value)
  {
    auto sectionMember = FindMember(RootObject(), section);
    auto entry = sectionMember && _text[sectionMember->ValueStart] == '{' ? FindMember(sectionMember->ValueStart, name) : nullopt;
    if (!entry || _text[entry->ValueStart] != '{') throw runtime_error(format("\"{}\" is not an object in the file.", name));

    if (auto member = FindMember(entry->ValueStart, property))
    {
      _text.replace(member->ValueStart, member->ValueEnd - member->ValueStart, value);
      return;
    }

    AddMember(entry->ValueStart, property, value, 2);
  }

  const string& JsonConfigEditor::Text() const
  {
    return _text;
  }

  size_t JsonConfigEditor::SkipSpace(size_t position) const
  {
    while (position < _text.size())
    {
      if (isspace(uint8_t(_text[position])))
      {
        position++;
      }
      else if (_text.compare(position, 2, "//") == 0)
      {
        position = _text.find('\n', position);
        if (position == string::npos) return _text.size();
      }
      else if (_text.compare(position, 2, "/*") == 0)
      {
        position = _text.find("*/", position + 2);
        if (position == string::npos) return _text.size();
        position += 2;
      }
      else
      {
        break;
      }
    }
    return position;
  }

  size_t JsonConfigEditor::StringEnd(size_t position) const
  {
    for (auto index = position + 1; index < _text.size(); index++)
    {
      if (_text[index] == '\\') index++;
      else if (_text[index] == '"') return index + 1;
    }
    throw runtime_error("The file has a string without its closing quote.");
  }

  size_t JsonConfigEditor::ValueEnd(size_t position) const
  {
    if (position >= _text.size()) throw runtime_error("The file ends where a value should be.");

    auto first = _text[position];
    if (first == '"') return StringEnd(position);
    if (first != '{' && first != '[')
    {
      auto end = position;
      while (end < _text.size() && !IsLiteralEnd(_text[end])) end++;
      return end;
    }

    //An object or an array: to its closing bracket, past strings and comments
    auto depth = 0;
    for (auto index = position; index < _text.size();)
    {
      auto character = _text[index];
      if (character == '"')
      {
        index = StringEnd(index);
        continue;
      }
      if (character == '/' && index + 1 < _text.size() && (_text[index + 1] == '/' || _text[index + 1] == '*'))
      {
        index = SkipSpace(index);
        continue;
      }

      if (character == '{' || character == '[') depth++;
      else if (character == '}' || character == ']') depth--;
      index++;
      if (depth == 0) return index;
    }
    throw runtime_error("The file has an object or array without its closing bracket.");
  }

  string JsonConfigEditor::StringAt(size_t position) const
  {
    string result;
    for (auto index = position + 1; index < _text.size() && _text[index] != '"'; index++)
    {
      if (_text[index] == '\\' && index + 1 < _text.size()) index++;
      result += _text[index];
    }
    return result;
  }

  size_t JsonConfigEditor::RootObject() const
  {
    auto position = SkipSpace(0);
    if (position >= _text.size() || _text[position] != '{') throw runtime_error("The file is not a JSON object.");
    return position;
  }

  optional<JsonConfigEditor::Member> JsonConfigEditor::FindMember(size_t objectStart, string_view name) const
  {
    for (auto& member : Members(objectStart))
    {
      if (StringAt(member.KeyStart) == name) return member;
    }
    return nullopt;
  }

  vector<JsonConfigEditor::Member> JsonConfigEditor::Members(size_t objectStart) const
  {
    vector<Member> members;
    auto position = SkipSpace(objectStart + 1);
    while (position < _text.size() && _text[position] != '}')
    {
      //JSONC allows a comma after the last member
      if (_text[position] == ',')
      {
        position = SkipSpace(position + 1);
        continue;
      }
      if (_text[position] != '"') throw runtime_error("The file has an object member without a quoted name.");

      auto colon = SkipSpace(StringEnd(position));
      if (colon >= _text.size() || _text[colon] != ':') throw runtime_error("The file has an object member without a colon.");

      auto valueStart = SkipSpace(colon + 1);
      auto valueEnd = ValueEnd(valueStart);
      members.push_back({ position, valueStart, valueEnd });
      position = SkipSpace(valueEnd);
    }
    if (position >= _text.size()) throw runtime_error("The file has an object without its closing brace.");
    return members;
  }

  void JsonConfigEditor::AddMember(size_t objectStart, string_view name, const string& value, size_t depth)
  {
    auto member = format("{}{}\"{}\": {}", LineBreak(), Indent(depth + 1), name, value);
    auto members = Members(objectStart);
    if (members.empty())
    {
      auto closing = ValueEnd(objectStart) - 1;
      _text.replace(objectStart, closing + 1 - objectStart, format("{{{}{}{}}}", member, LineBreak(), Indent(depth)));
      return;
    }

    //After the last member, and after its comma if it has one
    auto end = members.back().ValueEnd;
    auto next = SkipSpace(end);
    if (next < _text.size() && _text[next] == ',') _text.insert(next + 1, member);
    else _text.insert(end, "," + member);
  }

  string JsonConfigEditor::Indent(size_t depth) const
  {
    //As the file indents its first member: two spaces, a tab, or what it uses
    string unit = "  ";
    auto lineStart = _text.find('\n');
    while (lineStart != string::npos)
    {
      auto contentStart = _text.find_first_not_of(" \t", lineStart + 1);
      if (contentStart != string::npos && contentStart > lineStart + 1 && _text[contentStart] == '"')
      {
        unit = _text.substr(lineStart + 1, contentStart - lineStart - 1);
        break;
      }
      lineStart = _text.find('\n', lineStart + 1);
    }

    string result;
    for (size_t level = 0; level < depth; level++) result += unit;
    return result;
  }

  string JsonConfigEditor::Render(const JsonEntry& entry, size_t depth) const
  {
    string result = "{";
    for (size_t index = 0; index < entry.Properties.size(); index++)
    {
      auto& [name, value] = entry.Properties[index];
      result += format("{}{}\"{}\": {}{}", LineBreak(), Indent(depth + 1), name, value, index + 1 < entry.Properties.size() ? "," : "");
    }
    return result + format("{}{}}}", LineBreak(), Indent(depth));
  }

  string_view JsonConfigEditor::LineBreak() const
  {
    return _text.find("\r\n") != string::npos ? "\r\n" : "\n";
  }
}
