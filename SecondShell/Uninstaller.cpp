#include "pch.h"
#include "Uninstaller.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace SecondShell::Integrations;
using namespace SecondShell::Session;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Setup
{
  Uninstaller::Uninstaller(dependency_container* container) :
    _driverLauncher(container->resolve<DriverLauncher>()),
    _integrations(container->resolve<IntegrationManager>()),
    _credentials(container->resolve<CredentialStore>()),
    _machineSetup(container->resolve<MachineSetup>()),
    _environment(container->resolve<AppEnvironment>())
  { }

  UninstallReport Uninstaller::Run(const UninstallOptions& options) const
  {
    UninstallReport report;

    _driverLauncher->Disarm();
    RemoveIntegrations(report);

    if (options.ForgetPassword)
    {
      _credentials->Delete();
      report.Done.push_back("Forgot the saved password.");
    }

    if (options.DeleteData) DeleteData(report);

    report.IsSmoothSessionViewLeft = _machineSetup->IsSmoothSessionViewOn() && _machineSetup->IsSmoothSessionViewOurs();
    for (auto& problem : report.Problems) _logger.log(log_severity::warning, "{}", problem);
    return report;
  }

  void Uninstaller::RemoveIntegrations(UninstallReport& report) const
  {
    for (auto& integration : _integrations->Integrations())
    {
      auto state = integration->State();
      if (state != IntegrationState::Added && state != IntegrationState::Outdated) continue;

      try
      {
        integration->Remove();
        report.Done.push_back(format("Removed SecondShell from {}.", integration->Name()));
      }
      catch (const exception& exception)
      {
        report.Problems.push_back(format("Could not remove SecondShell from {}: {}", integration->Name(), exception.what()));
      }
    }
  }

  void Uninstaller::DeleteData(UninstallReport& report) const
  {
    //What is in use, e.g. this process's own log, stays; the rest goes
    auto folder = _environment->DataFolder();
    error_code error;
    if (!exists(folder, error)) return;

    size_t leftOver = 0;
    for (auto& item : directory_iterator(folder, error))
    {
      remove_all(item.path(), error);
      if (error) leftOver++;
    }
    remove(folder, error);

    if (leftOver == 0) report.Done.push_back(format("Deleted {}.", winrt::to_string(folder.wstring())));
    else report.Problems.push_back(format("Some files in {} are in use and stay; delete the folder once SecondShell has closed.", winrt::to_string(folder.wstring())));
  }
}
