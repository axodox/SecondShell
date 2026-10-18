#include "pch.h"
#include "App.xaml.h"
#include "AppEnvironment.h"
#include "McpRequestHandler.h"
#include "ProductInfo.h"

using namespace Axodox::Infrastructure;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Protocol;
using namespace ::SecondShell::Rendering;
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
    _window->UseServices(_container.get());
    auto windowHandle = Microsoft::UI::GetWindowFromWindowId(_window->AppWindow().Id());
    _container->resolve<CredentialProvider>()->SetPromptOwner(windowHandle);

    auto queue = DispatcherQueue::GetForCurrentThread();
    auto sessions = _container->resolve<SessionManager>();
    _stateChangedSubscription = sessions->StateChanged([queue, window = _window](SessionManager*, SessionState state) {
      queue.TryEnqueue([window, state] { window->ShowSessionState(state); });
      });

    _modeChangedSubscription = _container->resolve<AgentControl>()->ModeChanged([queue, window = _window](AgentControl*, ControlMode mode) {
      queue.TryEnqueue([window, mode] { window->ShowControlMode(mode); });
      });

    _runningChangedSubscription = _container->resolve<SequenceRunner>()->RunningChanged([queue, window = _window](SequenceRunner*, bool isRunning) {
      queue.TryEnqueue([window, isRunning] { window->ShowAgentRunning(isRunning); });
      });

    auto actionLog = _container->resolve<ActionLog>();
    _actionStartedSubscription = actionLog->ActionStarted([queue, window = _window](ActionLog*, const string& summary) {
      queue.TryEnqueue([window, summary] { window->ShowActionStarted(summary); });
      });
    _entryAddedSubscription = actionLog->EntryAdded([queue, window = _window](ActionLog*, const ActionLogEntry& entry) {
      queue.TryEnqueue([window, entry] { window->ShowActionLogged(entry); });
      });

    //The session view, until the window closes; closing it ends the session (Design.md, Session lifecycle)
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
