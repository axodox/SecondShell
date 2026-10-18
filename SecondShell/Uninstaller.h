#pragma once
#include "CredentialStore.h"
#include "DriverLauncher.h"
#include "IntegrationManager.h"
#include "MachineSetup.h"

namespace SecondShell::Setup
{
  struct UninstallOptions
  {
    bool ForgetPassword = false;
    bool DeleteData = false;  //captures, logs and settings
  };

  //What the uninstall did, and what it could not.
  struct UninstallReport
  {
    std::vector<std::string> Done;
    std::vector<std::string> Problems;
    bool IsSmoothSessionViewLeft = false;  //SecondShell set it; turning it off needs the elevated setup
  };

  //Removes what SecondShell added (Design.md, Uninstall): the driver's leftover RunOnce entry, its entries in the AI
  //apps, and, if asked, the saved password and the data folder. Child sessions stay enabled, as other tools use them.
  //The machine-wide smooth session view is reported, for the caller to turn off with the elevated setup.
  class Uninstaller
  {
    inline static const Axodox::Infrastructure::logger _logger{ "Uninstaller" };

  public:
    explicit Uninstaller(Axodox::Infrastructure::dependency_container* container);

    UninstallReport Run(const UninstallOptions& options) const;

  private:
    std::shared_ptr<Session::DriverLauncher> _driverLauncher;
    std::shared_ptr<Integrations::IntegrationManager> _integrations;
    std::shared_ptr<Session::CredentialStore> _credentials;
    std::shared_ptr<MachineSetup> _machineSetup;
    std::shared_ptr<Environment::AppEnvironment> _environment;

    void RemoveIntegrations(UninstallReport& report) const;
    void DeleteData(UninstallReport& report) const;
  };
}
