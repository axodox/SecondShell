#pragma once

namespace SecondShell::Integrations
{
  //A JSON value written out in a fixed order, e.g. { "command": "...", "args": [] }: properties with their JSON text.
  struct JsonEntry
  {
    std::vector<std::pair<std::string, std::string>> Properties;
  };

  //Edits one entry of a JSON config file as text, e.g. "secondshell" in "mcpServers": everything else in the file, its
  //order, formatting and comments (JSONC), stays as it is (Design.md, AI client integration).
  class JsonConfigEditor
  {
  public:
    explicit JsonConfigEditor(std::string text = {});

    //The entry's JSON text, if it is there.
    std::optional<std::string> Entry(std::string_view section, std::string_view name) const;
    void SetEntry(std::string_view section, std::string_view name, const JsonEntry& entry);
    void RemoveEntry(std::string_view section, std::string_view name);

    //The names of the section's entries, in their order.
    std::vector<std::string> EntryNames(std::string_view section) const;
    //Sets one property of an entry that is there, e.g. its "command", keeping its others (its "env", say).
    void SetEntryProperty(std::string_view section, std::string_view name, std::string_view property, const std::string& value);

    const std::string& Text() const;

  private:
    std::string _text;

    //Where a member of an object is: its key's opening quote, and its value
    struct Member
    {
      size_t KeyStart;
      size_t ValueStart;
      size_t ValueEnd;
    };

    size_t SkipSpace(size_t position) const;
    size_t ValueEnd(size_t position) const;
    size_t StringEnd(size_t position) const;
    std::string StringAt(size_t position) const;

    //The top-level object's opening brace; throws if the text is not an object.
    size_t RootObject() const;
    std::optional<Member> FindMember(size_t objectStart, std::string_view name) const;
    std::vector<Member> Members(size_t objectStart) const;

    //Adds a member at the end of the object, indented as its depth needs.
    void AddMember(size_t objectStart, std::string_view name, const std::string& value, size_t depth);
    std::string Indent(size_t depth) const;
    std::string Render(const JsonEntry& entry, size_t depth) const;
    std::string_view LineBreak() const;
  };
}
