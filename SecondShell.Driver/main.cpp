#include "pch.h"
#include "ActionExecutor.h"
#include "AppEnvironment.h"
#include "ClipboardSyncHandler.h"
#include "FramePublisher.h"
#include "UserInputHandler.h"
#include "KeyboardInjector.h"
#include "MouseInjector.h"
#include "ProductInfo.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Capture;
using namespace SecondShell::Desktop;
using namespace SecondShell::Driver;
using namespace SecondShell::Environment;
using namespace SecondShell::Input;
using namespace std;
using namespace std::chrono_literals;

//The composition root. Started by Windows at the child session's logon (the host's RunOnce entry); runs until the host
//disconnects. It exits at once if there is no host, or the host is not in another session, or the host refuses it.
int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*previousInstance*/, PWSTR /*commandLine*/, int /*showCommand*/)
{
  //Coordinates are physical pixels of the session
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  winrt::init_apartment();

  logger::file(AppEnvironment().LogFolder() / "driver.log");
  logger log{ "Driver" };
  DWORD sessionId = 0;
  ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
  log.log(log_severity::information, "{} driver {} started in session {}.", ProductInfo::Name(), ProductInfo::Version(), sessionId);

  try
  {
    dependency_container container;
    auto connection = container.resolve<HostConnection>();
    if (!connection->Open(10s)) return 1;

    //The handlers are added before the peer starts, but run only after the host has accepted this driver
    container.add<IMouseInjector, MouseInjector>();
    container.add<IKeyboardInjector, KeyboardInjector>();
    container.resolve<ActionExecutor>();
    container.resolve<FramePublisher>();
    container.resolve<UserInputHandler>();
    container.resolve<ClipboardSyncHandler>();

    if (!connection->Hello(10s)) return 1;

    container.resolve<ScreenCapturer>()->Start();
    connection->WaitUntilClosed();
    log.log(log_severity::information, "The host disconnected.");
  }
  catch (const exception& exception)
  {
    log.log(log_severity::fatal, "The driver failed: {}", exception.what());
    return 1;
  }
  catch (const winrt::hresult_error& error)
  {
    log.log(log_severity::fatal, "The driver failed: {}", winrt::to_string(error.message()));
    return 1;
  }

  return 0;
}
