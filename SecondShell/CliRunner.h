#pragma once

namespace SecondShell::Integrations
{
  struct CliResult
  {
    uint32_t ExitCode = 0;
    std::string Output;  //standard output and error, as they came
  };

  //Runs an AI app's own command-line tool (e.g. claude), hidden, and captures what it writes. Through cmd.exe, which
  //finds .cmd and .exe launchers on the PATH as a console would.
  class CliRunner
  {
  public:
    //The tool's name or path, and its arguments, each quoted as needed; empty if it did not finish in time.
    std::optional<CliResult> Run(std::string_view tool, std::span<const std::string> arguments, std::chrono::milliseconds timeout) const;

    //Whether the tool is on the PATH, as a .exe or .cmd.
    bool IsAvailable(std::string_view tool) const;

    static std::string CommandLine(std::string_view tool, std::span<const std::string> arguments);
  };
}
