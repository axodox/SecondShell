#include "pch.h"
#include "ElevatedSetupLauncher.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace std;

namespace SecondShell::Setup
{
  const wchar_t* const _setupSwitch = L"--setup";
  const wchar_t* const _childSessions = L"--child-sessions";
  const wchar_t* const _smoothViewOn = L"--smooth-view=on";
  const wchar_t* const _smoothViewOff = L"--smooth-view=off";

  wstring SetupRequest::ToArguments() const
  {
    wstring arguments = _setupSwitch;
    if (EnableChildSessions) arguments += format(L" {}", _childSessions);
    if (SmoothSessionView) arguments += format(L" {}", *SmoothSessionView ? _smoothViewOn : _smoothViewOff);
    return arguments;
  }

  optional<SetupRequest> SetupRequest::FromArguments(span<const wstring> arguments)
  {
    if (ranges::find(arguments, _setupSwitch) == arguments.end()) return nullopt;

    SetupRequest request;
    request.EnableChildSessions = ranges::find(arguments, _childSessions) != arguments.end();
    if (ranges::find(arguments, _smoothViewOn) != arguments.end()) request.SmoothSessionView = true;
    if (ranges::find(arguments, _smoothViewOff) != arguments.end()) request.SmoothSessionView = false;
    return request;
  }

  ElevatedSetupLauncher::ElevatedSetupLauncher(dependency_container* container) :
    _environment(container->resolve<AppEnvironment>())
  { }

  SetupOutcome ElevatedSetupLauncher::Run(HWND owner, const SetupRequest& request) const
  {
    auto executable = _environment->HostExecutable().wstring();
    auto arguments = request.ToArguments();

    SHELLEXECUTEINFOW execute{ .cbSize = sizeof(SHELLEXECUTEINFOW) };
    execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    execute.hwnd = owner;
    execute.lpVerb = L"runas";
    execute.lpFile = executable.c_str();
    execute.lpParameters = arguments.c_str();
    execute.nShow = SW_HIDE;

    if (!ShellExecuteExW(&execute))
    {
      auto error = GetLastError();
      if (error == ERROR_CANCELLED) return SetupOutcome::Declined;

      _logger.log(log_severity::error, "Could not start the elevated setup: {}", winrt::to_string(winrt::hresult_error(HRESULT_FROM_WIN32(error)).message()));
      return SetupOutcome::Failed;
    }

    winrt::handle process{ execute.hProcess };
    WaitForSingleObject(process.get(), INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(process.get(), &exitCode);
    if (exitCode != 0) _logger.log(log_severity::error, "The elevated setup failed with exit code {}.", exitCode);
    return exitCode == 0 ? SetupOutcome::Done : SetupOutcome::Failed;
  }
}
