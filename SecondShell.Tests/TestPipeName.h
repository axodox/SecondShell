#pragma once

namespace SecondShell::Tests
{
  //A pipe name no other test uses. Tests can start within one clock tick, and a server cannot open a pipe whose previous
  //instance is still being closed.
  inline std::string UniquePipeName(std::string_view purpose)
  {
    static std::atomic_uint32_t counter = 0;
    return std::format("SecondShell.Tests.{}.{}.{}", purpose, GetCurrentProcessId(), counter++);
  }
}
