#include "pch.h"
#include "HostLauncher.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace std;

namespace SecondShell::Mcp
{
  namespace
  {
    //Initializes COM on this thread, unless it already is.
    struct ComApartment
    {
      HRESULT Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

      ~ComApartment()
      {
        if (SUCCEEDED(Result)) CoUninitialize();
      }
    };

    //Raymond Chen's method: the shell of the desktop's view in the running Explorer, whose ShellExecute starts programs
    //as Explorer itself would.
    winrt::com_ptr<IShellDispatch2> DesktopShell()
    {
      auto windows = winrt::create_instance<IShellWindows>(CLSID_ShellWindows, CLSCTX_LOCAL_SERVER);

      VARIANT location{};
      location.vt = VT_I4;
      location.lVal = CSIDL_DESKTOP;
      VARIANT root{};
      long window = 0;
      winrt::com_ptr<IDispatch> desktop;
      winrt::check_hresult(windows->FindWindowSW(&location, &root, SWC_DESKTOP, &window, SWFO_NEEDDISPATCH, desktop.put()));
      if (!desktop) throw runtime_error("Explorer's desktop is not available.");

      winrt::com_ptr<IShellBrowser> browser;
      winrt::check_hresult(desktop.as<IServiceProvider>()->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(browser.put())));

      winrt::com_ptr<IShellView> view;
      winrt::check_hresult(browser->QueryActiveShellView(view.put()));

      winrt::com_ptr<IShellFolderViewDual> background;
      winrt::check_hresult(view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(background.put())));

      winrt::com_ptr<IDispatch> application;
      winrt::check_hresult(background->get_Application(application.put()));
      return application.as<IShellDispatch2>();
    }
  }

  HostLauncher::HostLauncher(dependency_container* container) :
    _environment(container->resolve<AppEnvironment>())
  { }

  void HostLauncher::Launch()
  {
    auto executable = _environment->HostExecutable();
    _logger.log(log_severity::information, "Starting {}.", executable.string());

    ComApartment apartment;
    auto shell = DesktopShell();
    winrt::check_hresult(shell->ShellExecute(_bstr_t(executable.c_str()), _variant_t(), _variant_t(executable.parent_path().c_str()), _variant_t(), _variant_t(long(SW_SHOWNOACTIVATE))));
  }
}
