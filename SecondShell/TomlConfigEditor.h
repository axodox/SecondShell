#pragma once

namespace SecondShell::Integrations
{
  //Edits one table of a TOML config file as text, e.g. [mcp_servers.secondshell] with its subtables: everything else in
  //the file stays as it is.
  class TomlConfigEditor
  {
  public:
    explicit TomlConfigEditor(std::string text = {});

    //The table's lines after its header, if it is there.
    std::optional<std::string> Table(std::string_view name) const;
    //Key and TOML value text, e.g. { "command", "'C:\\x.exe'" }.
    void SetTable(std::string_view name, const std::vector<std::pair<std::string, std::string>>& values);
    void RemoveTable(std::string_view name);

    const std::string& Text() const;

    //A TOML string of the text: a literal string, unless the text has a single quote.
    static std::string Quote(std::string_view text);

  private:
    std::string _text;

    //From the table's header to the next table that is not one of its subtables
    std::optional<std::pair<size_t, size_t>> Find(std::string_view name) const;
    std::string_view LineBreak() const;
  };
}
