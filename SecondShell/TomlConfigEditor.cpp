#include "pch.h"
#include "TomlConfigEditor.h"

using namespace std;

namespace SecondShell::Integrations
{
  namespace
  {
    //The table name of a header line, e.g. "mcp_servers.secondshell" for "[mcp_servers.secondshell]  # note"
    optional<string_view> HeaderOf(string_view line)
    {
      auto start = line.find_first_not_of(" \t");
      if (start == string_view::npos || line[start] != '[') return nullopt;

      //An array of tables, [[name]], counts as a header too
      auto nameStart = line.find_first_not_of("[ \t", start);
      auto nameEnd = line.find(']', nameStart);
      if (nameStart == string_view::npos || nameEnd == string_view::npos) return nullopt;

      auto name = line.substr(nameStart, nameEnd - nameStart);
      return name.substr(0, name.find_last_not_of(" \t") + 1);
    }

    bool IsWithin(string_view header, string_view name)
    {
      return header == name || (header.starts_with(name) && header.size() > name.size() && header[name.size()] == '.');
    }
  }

  TomlConfigEditor::TomlConfigEditor(string text) :
    _text(move(text))
  { }

  optional<string> TomlConfigEditor::Table(string_view name) const
  {
    auto range = Find(name);
    if (!range) return nullopt;

    auto bodyStart = _text.find('\n', range->first);
    bodyStart = bodyStart == string::npos || bodyStart > range->second ? range->second : bodyStart + 1;
    return _text.substr(bodyStart, range->second - bodyStart);
  }

  void TomlConfigEditor::SetTable(string_view name, const vector<pair<string, string>>& values)
  {
    auto lineBreak = LineBreak();
    auto table = format("[{}]{}", name, lineBreak);
    for (auto& [key, value] : values) table += format("{} = {}{}", key, value, lineBreak);

    if (auto range = Find(name))
    {
      if (range->second < _text.size()) table += lineBreak;
      _text.replace(range->first, range->second - range->first, table);
      return;
    }

    //At the end, after an empty line, as tables are usually separated
    if (!_text.empty() && !_text.ends_with('\n')) _text += lineBreak;
    if (!_text.empty()) _text += lineBreak;
    _text += table;
  }

  void TomlConfigEditor::RemoveTable(string_view name)
  {
    auto range = Find(name);
    if (!range) return;

    //With the empty line before it, which separated it from the table above
    auto start = range->first;
    if (start >= 2 && _text.compare(start - 2, 2, "\n\n") == 0) start--;
    else if (start >= 4 && _text.compare(start - 4, 4, "\r\n\r\n") == 0) start -= 2;
    _text.erase(start, range->second - start);
  }

  const string& TomlConfigEditor::Text() const
  {
    return _text;
  }

  string TomlConfigEditor::Quote(string_view text)
  {
    if (text.find('\'') == string_view::npos) return format("'{}'", text);

    string result = "\"";
    for (auto character : text)
    {
      if (character == '"' || character == '\\') result += '\\';
      result += character;
    }
    return result + "\"";
  }

  optional<pair<size_t, size_t>> TomlConfigEditor::Find(string_view name) const
  {
    optional<size_t> start;
    for (size_t lineStart = 0; lineStart < _text.size();)
    {
      auto lineEnd = _text.find('\n', lineStart);
      auto next = lineEnd == string::npos ? _text.size() : lineEnd + 1;

      auto header = HeaderOf(string_view{ _text }.substr(lineStart, next - lineStart));
      if (header && !start && IsWithin(*header, name)) start = lineStart;
      else if (header && start && !IsWithin(*header, name)) return pair{ *start, lineStart };

      lineStart = next;
    }

    if (start) return pair{ *start, _text.size() };
    return nullopt;
  }

  string_view TomlConfigEditor::LineBreak() const
  {
    return _text.find("\r\n") != string::npos ? "\r\n" : "\n";
  }
}
