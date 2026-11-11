#include "pch.h"
#include "DriverLauncher.h"
#include <winrt/Windows.ApplicationModel.h>
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <security.h>
#include <taskschd.h>
#include <comutil.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "comsuppw.lib")

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace winrt::Windows::ApplicationModel;
using namespace std;

namespace SecondShell::Session
{
  const wchar_t* const _runOnceKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
  const wchar_t* const _runOnceValue = L"SecondShellDriver";

  //The driver argument which tells it that a sign-in started it, so it exits at once where it is not wanted
  const wchar_t* const _logonArgument = L"--logon";

  //Normal priority: Task Scheduler's default, below normal, would slow down captures and input
  const int _taskPriority = 4;

  //The package's startup task (Packaging\AppxManifest.xml)
  const wchar_t* const _startupTaskId = L"SecondShellDriver";

  namespace
  {
    //COM on this thread for the call: the host's threads are of either kind
    class ComScope
    {
    public:
      ComScope() : _isOwned(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) { }
      ~ComScope() { if (_isOwned) CoUninitialize(); }

    private:
      bool _isOwned;
    };

    wstring UserName()
    {
      wchar_t name[256];
      ULONG size = ULONG(std::size(name));
      if (!GetUserNameExW(NameSamCompatible, name, &size)) winrt::throw_last_error();
      return name;
    }

    winrt::com_ptr<ITaskService> Connect()
    {
      auto service = winrt::create_instance<ITaskService>(CLSID_TaskScheduler, CLSCTX_INPROC_SERVER);
      winrt::check_hresult(service->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t()));
      return service;
    }

    //Task folders other than the root need administrator rights to create
    //The startup task API's waits cannot run on a UI thread
    template<typename TResult, typename TAction>
    TResult OffUiThread(TAction&& action)
    {
      TResult result{};
      exception_ptr error;
      thread([&] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        try { result = action(); }
        catch (...) { error = current_exception(); }
        winrt::uninit_apartment();
        }).join();
      if (error) rethrow_exception(error);
      return result;
    }

    winrt::com_ptr<ITaskFolder> RootFolder(const winrt::com_ptr<ITaskService>& service)
    {
      winrt::com_ptr<ITaskFolder> folder;
      winrt::check_hresult(service->GetFolder(_bstr_t(L"\\"), folder.put()));
      return folder;
    }
  }

  DriverLauncher::DriverLauncher(dependency_container* container) :
    _environment(container->resolve<AppEnvironment>()),
    _settings(container->resolve<SettingsStore>())
  {
    //A change of the choice applies at once, once the host has installed one
    _settingsChangedSubscription = _settings->Changed([this](SettingsStore*, const Settings& settings) {
      auto applied = _appliedChoice.load();
      if (applied != -1 && applied != int(settings.DriverStartupTask)) Install();
      });
  }

  bool DriverLauncher::Install()
  {
    auto useStartupTask = _settings->Current().DriverStartupTask;
    _appliedChoice = int(useStartupTask);

    //Packaged, the package's startup task starts it (unless the user turned it off in Task Manager); the scheduled task
    //only for the faster start, if chosen
    if (_environment->IsPackaged() && useStartupTask && IsStartupTaskEnabled())
    {
      RemoveScheduledTask();
      _logger.log(log_severity::information, "The package's startup task starts the driver.");
      return true;
    }

    return InstallScheduledTask();
  }

  void DriverLauncher::Uninstall()
  {
    RemoveScheduledTask();
  }

  void DriverLauncher::RemoveAll()
  {
    //The startup task goes with the package
    RemoveScheduledTask();
  }

  bool DriverLauncher::IsInstalled() const
  {
    return IsScheduled() || (_environment->IsPackaged() && IsStartupTaskEnabled());
  }

  bool DriverLauncher::IsStartupTaskEnabled() const
  {
    try
    {
      //GetAsync finds only the calling app's own tasks, and the startup task is the driver app's
      return OffUiThread<bool>([] {
        string found;
        for (auto task : StartupTask::GetForCurrentPackageAsync().get())
        {
          found += format(" {}={}", winrt::to_string(task.TaskId()), int(task.State()));
          if (task.TaskId() != _startupTaskId) continue;
          auto state = task.State();
          if (state == StartupTaskState::Enabled || state == StartupTaskState::EnabledByPolicy) return true;
        }
        _logger.log(log_severity::warning, "The package's startup task is not enabled; the package's tasks:{}", found.empty() ? " none" : found);
        return false;
        });
    }
    catch (const winrt::hresult_error& error)
    {
      _logger.log(log_severity::warning, "Could not read the state of the package's startup task: {}", winrt::to_string(error.message()));
      return false;
    }
  }

  wstring DriverLauncher::TaskName()
  {
    auto user = UserName();
    ranges::replace(user, L'\\', L' ');
    return format(L"SecondShell driver ({})", user);
  }

  bool DriverLauncher::InstallScheduledTask()
  {
    try
    {
      ComScope com;
      auto service = Connect();
      auto folder = RootFolder(service);

      winrt::com_ptr<ITaskDefinition> task;
      winrt::check_hresult(service->NewTask(0, task.put()));

      winrt::com_ptr<IRegistrationInfo> registration;
      winrt::check_hresult(task->get_RegistrationInfo(registration.put()));
      registration->put_Author(_bstr_t(L"SecondShell"));
      registration->put_Description(_bstr_t(L"Starts SecondShell's driver in the agent's desktop as it signs in. In any other session the driver exits at once."));

      //As the user, with their own rights, only while they are signed in
      auto user = UserName();
      winrt::com_ptr<IPrincipal> principal;
      winrt::check_hresult(task->get_Principal(principal.put()));
      winrt::check_hresult(principal->put_UserId(_bstr_t(user.c_str())));
      winrt::check_hresult(principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN));
      winrt::check_hresult(principal->put_RunLevel(TASK_RUNLEVEL_LUA));

      //Every sign-in starts its own instance, which runs as long as its session needs it, also on battery
      winrt::com_ptr<ITaskSettings> settings;
      winrt::check_hresult(task->get_Settings(settings.put()));
      winrt::check_hresult(settings->put_MultipleInstances(TASK_INSTANCES_PARALLEL));
      winrt::check_hresult(settings->put_ExecutionTimeLimit(_bstr_t(L"PT0S")));
      winrt::check_hresult(settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE));
      winrt::check_hresult(settings->put_StopIfGoingOnBatteries(VARIANT_FALSE));
      winrt::check_hresult(settings->put_StartWhenAvailable(VARIANT_FALSE));
      winrt::check_hresult(settings->put_Priority(_taskPriority));

      //At every sign-in of this user: the child session's, and the user's own, where the driver exits at once
      winrt::com_ptr<ITriggerCollection> triggers;
      winrt::check_hresult(task->get_Triggers(triggers.put()));
      winrt::com_ptr<ITrigger> trigger;
      winrt::check_hresult(triggers->Create(TASK_TRIGGER_LOGON, trigger.put()));
      auto logonTrigger = trigger.as<ILogonTrigger>();
      winrt::check_hresult(logonTrigger->put_UserId(_bstr_t(user.c_str())));

      winrt::com_ptr<IActionCollection> actions;
      winrt::check_hresult(task->get_Actions(actions.put()));
      winrt::com_ptr<IAction> action;
      winrt::check_hresult(actions->Create(TASK_ACTION_EXEC, action.put()));
      auto execAction = action.as<IExecAction>();
      winrt::check_hresult(execAction->put_Path(_bstr_t(_environment->DriverExecutable().c_str())));
      winrt::check_hresult(execAction->put_Arguments(_bstr_t(_logonArgument)));

      winrt::com_ptr<IRegisteredTask> registered;
      winrt::check_hresult(folder->RegisterTaskDefinition(_bstr_t(TaskName().c_str()), task.get(), TASK_CREATE_OR_UPDATE,
        _variant_t(), _variant_t(), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t(L""), registered.put()));

      _logger.log(log_severity::information, "Registered the driver's sign-in task for {}.", winrt::to_string(_environment->DriverExecutable().wstring()));
      return true;
    }
    catch (const winrt::hresult_error& error)
    {
      _logger.log(log_severity::warning, "Could not register the driver's sign-in task, so a RunOnce entry starts the driver instead, later: {}", winrt::to_string(error.message()));
      return false;
    }
  }

  void DriverLauncher::RemoveScheduledTask()
  {
    try
    {
      ComScope com;
      if (SUCCEEDED(RootFolder(Connect())->DeleteTask(_bstr_t(TaskName().c_str()), 0))) _logger.log(log_severity::information, "Removed the driver's sign-in task.");
    }
    catch (const winrt::hresult_error& error)
    {
      _logger.log(log_severity::warning, "Could not remove the driver's sign-in task: {}", winrt::to_string(error.message()));
    }
  }

  bool DriverLauncher::IsScheduled() const
  {
    try
    {
      ComScope com;
      winrt::com_ptr<IRegisteredTask> task;
      if (FAILED(RootFolder(Connect())->GetTask(_bstr_t(TaskName().c_str()), task.put()))) return false;

      VARIANT_BOOL isEnabled = VARIANT_FALSE;
      return SUCCEEDED(task->get_Enabled(&isEnabled)) && isEnabled;
    }
    catch (const winrt::hresult_error&)
    {
      return false;
    }
  }

  void DriverLauncher::Arm()
  {
    //Two drivers would start; the host accepts one, the other exits, but the RunOnce one would run late for nothing
    if (IsInstalled()) return;

    auto command = format(L"\"{}\"", _environment->DriverExecutable().wstring());
    winrt::check_win32(RegSetKeyValueW(HKEY_CURRENT_USER, _runOnceKey, _runOnceValue, REG_SZ, command.c_str(), DWORD((command.size() + 1) * sizeof(wchar_t))));
    _logger.log(log_severity::information, "Armed the driver's RunOnce entry.");
  }

  void DriverLauncher::Disarm()
  {
    if (RegDeleteKeyValueW(HKEY_CURRENT_USER, _runOnceKey, _runOnceValue) == ERROR_SUCCESS)
    {
      _logger.log(log_severity::information, "Removed the driver's RunOnce entry.");
    }
  }

  bool DriverLauncher::IsArmed() const
  {
    return RegGetValueW(HKEY_CURRENT_USER, _runOnceKey, _runOnceValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
  }
}
