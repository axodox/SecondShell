#include "pch.h"
#include "ActionExecutor.h"
#include "AppEnvironment.h"
#include "AutomationTextSource.h"
#include "ClipboardSyncHandler.h"
#include "FramePublisher.h"
#include "UserInputHandler.h"
#include "KeyboardInjector.h"
#include "MouseInjector.h"
#include "PenInjector.h"
#include "PipeNames.h"
#include "ProductInfo.h"
#include "TouchInjector.h"
#include "WindowsTextRecognizer.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Capture;
using namespace SecondShell::Desktop;
using namespace SecondShell::Driver;
using namespace SecondShell::Environment;
using namespace SecondShell::Input;
using namespace std;
using namespace std::chrono_literals;

namespace
{
  //Started at a sign-in, the driver runs while Windows still shows its sign-in desktop (Winlogon), which a user's
  //process cannot open; capturing the screen before the user's desktop (Default) takes input blocks. Waits until it
  //does, at most this long, then starts anyway
  bool WaitForUserDesktop(std::chrono::milliseconds timeout)
  {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true)
    {
      if (auto desktop = OpenInputDesktop(0, FALSE, GENERIC_READ))
      {
        wchar_t name[64] = {};
        auto hasName = GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), nullptr);
        CloseDesktop(desktop);
        if (hasName && _wcsicmp(name, L"Default") == 0) return true;
      }

      if (std::chrono::steady_clock::now() > deadline) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  //Whether the host's driver pipe exists, without waiting for it
  bool IsHostListening()
  {
    auto pipe = wstring(L"\\\\.\\pipe\\") + wstring(winrt::to_hstring(PipeNames::Driver()));
    return WaitNamedPipeW(pipe.c_str(), 1) || GetLastError() != ERROR_FILE_NOT_FOUND;
  }
}

//The composition root. Started by Windows at every sign-in of the user (the host's scheduled task, with --logon), or at
//the child session's sign-in (the host's RunOnce entry, if the task is missing); runs until the host disconnects. It
//exits at once if there is no host, or the host is in this session (not a child session), or the host refuses it.
int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*previousInstance*/, PWSTR commandLine, int /*showCommand*/)
{
  //At a sign-in without a host, e.g. the user's own after a restart, there is nothing to do: not even a log line
  auto isLogon = wstring_view(commandLine).find(L"--logon") != wstring_view::npos;
  if (isLogon && !IsHostListening()) return 0;

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
    //Started at a sign-in, the host is there already; else it may still be starting
    if (!connection->Open(isLogon ? 3s : 10s)) return 1;

    //Text waits: UI Automation and Windows OCR on window crops in the quick tier, Windows OCR on bands upscaled 1.5x in
    //the thorough one (OcrOptions.md, measured)
    container.add<QuickTextReader>([] { return make_shared<QuickTextReader>(make_shared<AutomationTextSource>(), make_shared<WindowsTextRecognizer>()); });
    container.add<ThoroughTextReader>([] { return make_shared<ThoroughTextReader>(make_shared<WindowsTextRecognizer>(1.5)); });

    //The handlers are added before the peer starts, but run only after the host has accepted this driver
    container.add<IMouseInjector, MouseInjector>();
    container.add<IKeyboardInjector, KeyboardInjector>();
    container.add<ITouchInjector, TouchInjector>();
    container.add<IPenInjector, PenInjector>();
    container.resolve<ActionExecutor>();
    container.resolve<FramePublisher>();
    container.resolve<UserInputHandler>();
    container.resolve<ClipboardSyncHandler>();

    //Started at a sign-in: the session is usable, and the host says Ready, once the user's desktop is up (a few
    //seconds); the host may still hold the hello until it knows the session is its child session
    if (isLogon)
    {
      auto start = std::chrono::steady_clock::now();
      auto isReady = WaitForUserDesktop(60s);
      auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
      log.log(isReady ? log_severity::information : log_severity::warning, "{} after {} ms.", isReady ? "The user's desktop is up" : "No sign of the user's desktop", waited.count());
    }
    if (!connection->Hello(isLogon ? 30s : 10s)) return 1;

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
