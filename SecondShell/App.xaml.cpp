#include "pch.h"
#include "App.xaml.h"
#include "AppEnvironment.h"
#include "CaptureStore.h"
#include "ClipboardSync.h"
#include "MachineSetup.h"
#include "McpRequestHandler.h"
#include "ProductInfo.h"
#include "SetupChecker.h"

using namespace Axodox::Infrastructure;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Protocol;
using namespace ::SecondShell::Rendering;
using namespace ::SecondShell::Session;
using namespace ::SecondShell::Setup;
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

    auto argumentCount = 0;
    auto argumentList = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    vector<wstring> arguments{ argumentList + min(argumentCount, 1), argumentList + argumentCount };
    LocalFree(argumentList);

    //Through exit, so the log is written out
    if (auto request = SetupRequest::FromArguments(arguments)) exit(RunSetup(*request));

    logger{ "App" }.log(log_severity::information, "{} host {} started.", ProductInfo::Name(), ProductInfo::Version());

    RegisterServices();
    if (!StartServices())
    {
      Exit();
      return;
    }

    ShowWindow();
  }

  int App::RunSetup(const SetupRequest& request)
  {
    logger log{ "Setup" };
    log.log(log_severity::information, "{} {} setup started.", ProductInfo::Name(), ProductInfo::Version());

    try
    {
      dependency_container container;
      container.add<IMachineConfiguration, MachineConfiguration>();

      auto setup = container.resolve<MachineSetup>();
      setup->EnableChildSessions();
      if (request.SmoothSessionView) setup->SetSmoothSessionView(*request.SmoothSessionView);
      return 0;
    }
    catch (const winrt::hresult_error& error)
    {
      log.log(log_severity::error, "The setup failed: {}", winrt::to_string(error.message()));
    }
    catch (const exception& exception)
    {
      log.log(log_severity::error, "The setup failed: {}", exception.what());
    }
    return 1;
  }

  void App::RegisterServices()
  {
    _container = make_unique<dependency_container>();
    _container->add<IDriverLink, DriverLink>();
    _container->add<IMachineConfiguration, MachineConfiguration>();
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

    //Per-user problems are fixed here and now; machine-wide ones the window offers to set up
    _container->resolve<SetupChecker>()->Check();

    //Old capture folders go in the background; there may be many files
    thread([captures = _container->resolve<CaptureStore>()] { captures->RemoveOldCaptures(); }).detach();
    _container->resolve<DriverLink>()->Start();
    _container->resolve<ClipboardSync>();
    return true;
  }

  void App::ShowWindow()
  {
    _window = make_self<MainWindow>();
    _window->UseServices(_container.get());
    auto windowHandle = Microsoft::UI::GetWindowFromWindowId(_window->AppWindow().Id());
    _container->resolve<CredentialProvider>()->SetPromptOwner(windowHandle);

    //The session view, until the window closes; closing it ends the session (Design.md, Session lifecycle)
    auto sessions = _container->resolve<SessionManager>();
    auto renderer = _container->resolve<SessionRenderer>();
    renderer->Attach(_window->SessionView(), windowHandle);
    _window->Closed([sessions, renderer](const IInspectable&, const WindowEventArgs&) {
      renderer->Detach();
      sessions->Shutdown();
      });

    //Shown without taking the focus from the user (Design.md, Working without focus)
    _window->AppWindow().Show(false);
  }
}
