#include "pch.h"
#include "App.xaml.h"
#include "AppEnvironment.h"
#include "CaptureStore.h"
#include "ClipboardSync.h"
#include "IntegrationManager.h"
#include "MachineSetup.h"
#include "McpRequestHandler.h"
#include "ProductInfo.h"
#include "SetupChecker.h"
#include "Uninstaller.h"

using namespace Axodox::Infrastructure;
using namespace ::SecondShell::Agent;
using namespace ::SecondShell::Environment;
using namespace ::SecondShell::Integrations;
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
    auto argumentCount = 0;
    auto argumentList = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    vector<wstring> arguments{ argumentList + min(argumentCount, 1), argumentList + argumentCount };
    LocalFree(argumentList);

    //Before the log opens, which would keep a file of the data folder in use
    if (auto exitCode = RunUninstall(arguments)) exit(*exitCode);
    logger::file(AppEnvironment().LogFolder() / "host.log");

    //Through exit, so the log is written out
    if (auto request = SetupRequest::FromArguments(arguments)) exit(RunSetup(*request));
    if (auto exitCode = RunIntegrationCommand(arguments)) exit(*exitCode);

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
      if (request.EnableChildSessions) setup->EnableChildSessions();
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

  std::optional<int> App::RunIntegrationCommand(span<const wstring> arguments)
  {
    auto command = ranges::find_if(arguments, [](const wstring& argument) { return argument == L"--integrate" || argument == L"--unintegrate"; });
    if (command == arguments.end()) return std::nullopt;

    UseParentConsole();

    dependency_container container;
    auto integrations = container.resolve<IntegrationManager>();
    auto isAdding = *command == L"--integrate";
    auto target = command + 1 == arguments.end() ? string{} : to_string(*(command + 1));

    auto integration = integrations->Find(target);
    if (!integration)
    {
      printf("Usage: SecondShell.exe --integrate|--unintegrate <app>\nApps:");
      for (auto& item : integrations->Integrations()) printf(" %.*s", int(item->Id().size()), item->Id().data());
      printf("\n");
      return 2;
    }

    try
    {
      if (isAdding) integration->Add();
      else integration->Remove();

      auto done = std::format("{} {}.", isAdding ? "Added SecondShell to" : "Removed SecondShell from", integration->Name());
      printf("%s %s\n", done.c_str(), integration->PreviewAdd().Note.c_str());
      logger{ "App" }.log(log_severity::information, "{} (from the command line)", done);
      return 0;
    }
    catch (const winrt::hresult_error& error)
    {
      printf("%s\n", to_string(error.message()).c_str());
    }
    catch (const exception& exception)
    {
      printf("%s\n", exception.what());
    }
    return 1;
  }

  std::optional<int> App::RunUninstall(span<const wstring> arguments)
  {
    auto has = [&](const wchar_t* argument) { return ranges::find(arguments, argument) != arguments.end(); };
    if (!has(L"--uninstall")) return std::nullopt;

    UseParentConsole();
    dependency_container container;
    container.add<IMachineConfiguration, MachineConfiguration>();

    try
    {
      auto report = container.resolve<Uninstaller>()->Run({ .ForgetPassword = has(L"--forget-password"), .DeleteData = has(L"--delete-data") });
      for (auto& done : report.Done) printf("%s\n", done.c_str());

      //Machine-wide: the elevated setup turns it off
      if (report.IsSmoothSessionViewLeft)
      {
        auto outcome = container.resolve<ElevatedSetupLauncher>()->Run(nullptr, { .SmoothSessionView = false });
        if (outcome == SetupOutcome::Done) printf("Turned the smooth session view off.\n");
        else report.Problems.push_back("The smooth session view stays on: it needs administrator rights to turn off.");
      }

      for (auto& problem : report.Problems) printf("%s\n", problem.c_str());
      if (!has(L"--forget-password") || !has(L"--delete-data")) printf("Kept the saved password and the data folder unless asked: --forget-password, --delete-data.\n");
      printf("Child sessions stay enabled; other tools may use them. The SecondShell folder can be deleted now.\n");
      return report.Problems.empty() ? 0 : 1;
    }
    catch (const winrt::hresult_error& error)
    {
      printf("The uninstall failed: %s\n", to_string(error.message()).c_str());
    }
    catch (const exception& exception)
    {
      printf("The uninstall failed: %s\n", exception.what());
    }
    return 1;
  }

  void App::UseParentConsole()
  {
    //A window app has no console of its own: it writes to the one it was started from, if there is one
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
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
