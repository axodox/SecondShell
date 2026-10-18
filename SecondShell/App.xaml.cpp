#include "pch.h"
#include "App.xaml.h"
#include "AppEnvironment.h"
#include "McpRequestHandler.h"
#include "ProductInfo.h"

using namespace Axodox::Infrastructure;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Protocol;
using namespace ::SecondShell::Session;
using namespace std;
using namespace winrt;
using namespace winrt::Microsoft::UI::Dispatching;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::SecondShell::implementation
{
  App::App()
  {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
    UnhandledException([](const IInspectable&, const UnhandledExceptionEventArgs&) {
      if (IsDebuggerPresent()) __debugbreak();
    });
#endif
  }

  void App::OnLaunched(const LaunchActivatedEventArgs& /*eventArgs*/)
  {
    logger::file(AppEnvironment().LogFolder() / "host.log");
    logger{ "App" }.log(log_severity::information, "{} host {} started.", ProductInfo::Name(), ProductInfo::Version());

    RegisterServices();
    if (!StartServices())
    {
      Exit();
      return;
    }

    ShowWindow();
  }

  void App::RegisterServices()
  {
    _container = make_unique<dependency_container>();
    _container->add<IDriverLink, DriverLink>();
  }

  bool App::StartServices()
  {
    //Single instance: the first host claims the MCP pipe
    try
    {
      _container->resolve<McpRequestHandler>()->Start();
    }
    catch (const exception& exception)
    {
      logger{ "App" }.log(log_severity::warning, "Another SecondShell host is running ({}); exiting.", exception.what());
      return false;
    }

    //A RunOnce entry left by a host that crashed would start the driver at the user's next logon
    _container->resolve<DriverLauncher>()->Disarm();
    _container->resolve<DriverLink>()->Start();
    return true;
  }

  void App::ShowWindow()
  {
    _window = make_self<MainWindow>();
    _container->resolve<CredentialProvider>()->SetPromptOwner(Microsoft::UI::GetWindowFromWindowId(_window->AppWindow().Id()));

    auto queue = DispatcherQueue::GetForCurrentThread();
    auto sessions = _container->resolve<SessionManager>();
    _stateChangedSubscription = sessions->StateChanged([queue, window = _window](SessionManager*, SessionState state) {
      queue.TryEnqueue([window, state] { window->ShowSessionState(state); });
      });

    //Closing the window ends the session (Design.md, Session lifecycle)
    _window->Closed([sessions](const IInspectable&, const WindowEventArgs&) { sessions->Shutdown(); });

    //Shown without taking the focus from the user (Design.md, Working without focus)
    _window->AppWindow().Show(false);
  }
}
