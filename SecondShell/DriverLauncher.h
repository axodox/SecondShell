#pragma once
#include "AppEnvironment.h"

namespace SecondShell::Session
{
  //Starts the driver in a new child session with a RunOnce entry, which Windows runs at the session's logon and then
  //deletes (Design.md, Starting the driver).
  class DriverLauncher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "DriverLauncher" };

  public:
    explicit DriverLauncher(Axodox::Infrastructure::dependency_container* container);

    //Before connecting to a new child session.
    void Arm();

    //When no logon will consume the entry: the connection failed, or on startup after a crash.
    void Disarm();
    bool IsArmed() const;

  private:
    std::shared_ptr<Environment::AppEnvironment> _environment;
  };
}
