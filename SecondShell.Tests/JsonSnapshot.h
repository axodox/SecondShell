#pragma once

namespace SecondShell::Tests
{
  //Compares JSON with a snapshot file in SecondShell.Tests\Snapshots, written with sorted keys so the comparison does
  //not depend on member order. A missing snapshot is created and the test fails, so it gets reviewed; set
  //SECONDSHELL_UPDATE_SNAPSHOTS=1 to rewrite the snapshots after an intended change.
  class JsonSnapshot
  {
  public:
    static std::string ToCanonicalString(const Axodox::Json::json_value& json);
    static void AssertMatches(std::string_view name, const Axodox::Json::json_value& json);

    //Compares JSON with the expected JSON text, ignoring member order and formatting.
    static void AssertEqual(std::string_view expected, const Axodox::Json::json_value& actual);

  private:
    static void Write(std::string& text, const Axodox::Json::json_value& json, size_t depth);
  };
}
