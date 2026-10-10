#include "pch.h"
#include "SpikeRunner.h"
#include "FrameReader.h"

#pragma comment(lib, "credui.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "version.lib")

using namespace winrt::Microsoft::UI::Dispatching;

namespace
{
  const wchar_t* RunOnceKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
  const wchar_t* RunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  const wchar_t* RunOnceValue = L"SecondShellSpike";

  //Runs work on the thread that owns the RDP control.
  struct RdpThread
  {
    virtual ~RdpThread() = default;
    virtual void Invoke(const std::function<void()>& action) = 0;
  };

  //The WinUI 3 UI thread, through its DispatcherQueue.
  class UiRdpThread : public RdpThread
  {
    DispatcherQueue _queue;

  public:
    explicit UiRdpThread(DispatcherQueue queue) : _queue(std::move(queue)) {}

    void Invoke(const std::function<void()>& action) override
    {
      std::promise<void> done;
      _queue.TryEnqueue([&] {
        try { action(); done.set_value(); }
        catch (...) { done.set_exception(std::current_exception()); }
      });
      done.get_future().get();
    }
  };

  //A dedicated STA thread with its own message loop.
  class DedicatedRdpThread : public RdpThread
  {
    std::thread _thread;
    DWORD _threadId = 0;

  public:
    DedicatedRdpThread()
    {
      std::promise<void> ready;
      _thread = std::thread([&] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        MSG message;
        PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        _threadId = GetCurrentThreadId();
        ready.set_value();

        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
          if (!message.hwnd && message.message == WM_APP)
          {
            (*reinterpret_cast<std::function<void()>*>(message.lParam))();
            continue;
          }
          TranslateMessage(&message);
          DispatchMessageW(&message);
        }
        CoUninitialize();
      });
      ready.get_future().wait();
    }

    ~DedicatedRdpThread() override
    {
      PostThreadMessageW(_threadId, WM_QUIT, 0, 0);
      _thread.join();
    }

    void Invoke(const std::function<void()>& action) override
    {
      std::promise<void> done;
      std::function<void()> task = [&] {
        try { action(); done.set_value(); }
        catch (...) { done.set_exception(std::current_exception()); }
      };
      PostThreadMessageW(_threadId, WM_APP, 0, reinterpret_cast<LPARAM>(&task));
      done.get_future().get();
    }
  };

  std::string ApartmentOfCurrentThread()
  {
    APTTYPE type;
    APTTYPEQUALIFIER qualifier;
    if (FAILED(CoGetApartmentType(&type, &qualifier))) return "none";
    const char* names[] = { "STA", "MTA", "NA", "MainSTA" };
    return std::format("{} (qualifier {})", type >= 0 && type <= 3 ? names[type] : "?", int(qualifier));
  }

  std::string ModuleVersion(const wchar_t* moduleName)
  {
    auto module = GetModuleHandleW(moduleName);
    if (!module) return "not loaded";
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(module, path, MAX_PATH);
    DWORD handle = 0;
    auto size = GetFileVersionInfoSizeW(path, &handle);
    std::vector<uint8_t> data(size);
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (!size || !GetFileVersionInfoW(path, 0, size, data.data()) || !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length)) return "?";
    return std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS), HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
  }

  std::string WindowsVersion()
  {
    wchar_t build[32]{}, display[32]{};
    DWORD ubr = 0, size = sizeof(build);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber", RRF_RT_REG_SZ, nullptr, build, &size);
    size = sizeof(display);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion", RRF_RT_REG_SZ, nullptr, display, &size);
    size = sizeof(ubr);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR", RRF_RT_REG_DWORD, nullptr, &ubr, &size);
    return std::format("{}.{} ({})", Spike::Narrow(build), ubr, Spike::Narrow(display));
  }

  std::optional<uint32_t> ExistingChildSession()
  {
    ULONG id = 0;
    if (!WTSGetChildSessionId(&id) || id == 0 || id == ULONG(-1)) return std::nullopt;
    return uint32_t(id);
  }

  std::string ForegroundDescription()
  {
    auto window = GetForegroundWindow();
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    wchar_t className[128]{};
    GetClassNameW(window, className, 128);
    return std::format("hwnd={} pid={} class='{}'{}", static_cast<void*>(window), processId, Spike::Narrow(className), processId == GetCurrentProcessId() ? " (THIS PROCESS)" : "");
  }

  //Names of the processes running in a session, e.g. to see whether the shell has started.
  std::string SessionProcesses(uint32_t sessionId)
  {
    PWTS_PROCESS_INFOW processes = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &processes, &count)) return "?";
    std::map<std::string, int> names;
    for (DWORD i = 0; i < count; i++) if (processes[i].SessionId == sessionId) names[Spike::Narrow(processes[i].pProcessName)]++;
    WTSFreeMemory(processes);
    std::string result;
    for (auto& [name, number] : names) result += number > 1 ? std::format("{}x{} ", name, number) : name + " ";
    return result;
  }
}

SpikeOptions SpikeOptions::Parse()
{
  int argc;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  SpikeOptions options;
  options.Thread = Spike::ArgValue(argc, argv, L"--thread", options.Thread);
  options.Window = ParseHostWindowMode(Spike::ArgValue(argc, argv, L"--window", L"never"));
  options.WinUiWindow = Spike::ArgValue(argc, argv, L"--winui", options.WinUiWindow);
  options.Run = Spike::ArgValue(argc, argv, L"--run");
  options.TimeoutSeconds = std::stoi(Spike::ArgValue(argc, argv, L"--timeout", L"180"));
  options.Width = std::stoi(Spike::ArgValue(argc, argv, L"--width", L"1920"));
  options.Height = std::stoi(Spike::ArgValue(argc, argv, L"--height", L"1080"));
  options.Frames = Spike::HasArg(argc, argv, L"--frames");
  options.FrameSeconds = std::stoi(Spike::ArgValue(argc, argv, L"--frame-seconds", L"20"));
  options.Adapter = std::stoi(Spike::ArgValue(argc, argv, L"--adapter", L"0"));
  options.Label = Spike::ArgValue(argc, argv, L"--label", options.Label);
  options.Startup = Spike::ArgValue(argc, argv, L"--startup", options.Startup);
  options.ClassVersion = std::stoi(Spike::ArgValue(argc, argv, L"--class", L"9"));
  LocalFree(argv);
  return options;
}

std::string SpikeOptions::Describe() const
{
  return std::format("label={} class={} startup={} thread={} window={} winui={} run='{}' timeout={} size={}x{} frames={} adapter={}",
    Spike::Narrow(Label), ClassVersion, Spike::Narrow(Startup), Spike::Narrow(Thread), ToString(Window), Spike::Narrow(WinUiWindow), Spike::Narrow(Run), TimeoutSeconds, Width, Height, Frames, Adapter);
}

SpikeRunner::SpikeRunner(SpikeOptions options, DispatcherQueue uiQueue, std::function<void(const std::string&)> uiLog) :
  _options(std::move(options)),
  _uiQueue(std::move(uiQueue)),
  _uiLog(std::move(uiLog)),
  _log(Spike::OutDir() / L"host.log")
{}

SpikeRunner::~SpikeRunner()
{
  if (_worker.joinable()) _worker.join();
}

void SpikeRunner::Log(const std::string& message)
{
  _log(message);
  _uiQueue.TryEnqueue([this, message] { _uiLog(message); });
}

void SpikeRunner::Start(HWND winUiWindow)
{
  _winUiWindow = winUiWindow;
  _worker = std::thread([this] {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try
    {
      Execute();
    }
    catch (const std::exception& error)
    {
      Log(std::format("HOST ERROR: {}", error.what()));
    }
    catch (const winrt::hresult_error& error)
    {
      Log(std::format("HOST ERROR: 0x{:08X} {}", uint32_t(error.code()), winrt::to_string(error.message())));
    }
    Log("HOST DONE");
    _uiQueue.TryEnqueue([] { winrt::Microsoft::UI::Xaml::Application::Current().Exit(); });
    CoUninitialize();
  });
}

void SpikeRunner::Execute()
{
  auto outDir = Spike::OutDir();
  Log("==== HOST START " + _options.Describe());
  Log(std::format("Windows {}, host pid {} session {}", WindowsVersion(), GetCurrentProcessId(), Spike::CurrentSessionId()));
  Log(std::format("Microsoft.UI.Xaml.dll {}, Microsoft.WindowsAppRuntime.dll {}, mstscax.dll (after creation)", ModuleVersion(L"Microsoft.UI.Xaml.dll"), ModuleVersion(L"Microsoft.WindowsAppRuntime.dll")));
  for (auto& adapter : EnumerateAdapters()) Log(std::format("Adapter: {} luid {:08X}:{:08X}", adapter.Name, uint32_t(adapter.Luid.HighPart), adapter.Luid.LowPart));
  Log("Foreground at start: " + ForegroundDescription());

  //Never touch a child session someone else started
  if (auto existing = ExistingChildSession())
  {
    Log(std::format("ABORT: a child session ({}) already exists", *existing));
    return;
  }

  for (auto name : { L"child.log", L"expected_session.txt", L"frames.txt", L"host.txt", L"frame_host.png", L"frame_child.png" })
  {
    std::error_code error;
    std::filesystem::remove(outDir / name, error);
  }

  //Frame reader adapter, announced to the writer
  AdapterInfo readerAdapter;
  if (_options.Frames)
  {
    auto adapters = EnumerateAdapters();
    readerAdapter = adapters.at(_options.Adapter);
    Spike::WriteAll(outDir / L"host.txt", std::format("host_pid={}\nadapter_luid_high={}\nadapter_luid_low={}\nadapter_name={}\n",
      GetCurrentProcessId(), readerAdapter.Luid.HighPart, readerAdapter.Luid.LowPart, readerAdapter.Name));
    Log(std::format("Frame reader adapter: {}", readerAdapter.Name));
  }

  //RunOnce (or Run) entry for the program in the child session
  auto startupKey = _options.Startup == L"run" ? RunKey : RunOnceKey;
  if (!_options.Run.empty())
  {
    auto result = RegSetKeyValueW(HKEY_CURRENT_USER, startupKey, RunOnceValue, REG_SZ, _options.Run.c_str(), DWORD((_options.Run.size() + 1) * sizeof(wchar_t)));
    Log(std::format("{} armed: {}", Spike::Narrow(startupKey), result == ERROR_SUCCESS ? "OK" : std::to_string(result)));
  }

  //Credentials
  std::wstring user, domain, password;
  {
    PCREDENTIALW credential;
    if (!CredReadW(L"SecondShell:ChildSession", CRED_TYPE_GENERIC, 0, &credential)) throw std::runtime_error("No stored credential SecondShell:ChildSession");
    wchar_t name[CREDUI_MAX_USERNAME_LENGTH + 1]{}, domainName[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1]{};
    if (CredUIParseUserNameW(credential->UserName, name, ARRAYSIZE(name), domainName, ARRAYSIZE(domainName)) != NO_ERROR) wcscpy_s(name, credential->UserName);
    user = name;
    domain = domainName;
    password.assign(reinterpret_cast<wchar_t*>(credential->CredentialBlob), credential->CredentialBlobSize / sizeof(wchar_t));
    CredFree(credential);
  }

  //RDP control on the chosen thread
  std::unique_ptr<RdpThread> rdpThread;
  if (_options.Thread == L"dedicated") rdpThread = std::make_unique<DedicatedRdpThread>();
  else rdpThread = std::make_unique<UiRdpThread>(_uiQueue);

  std::atomic<bool> loginComplete = false, disconnected = false;
  std::atomic<long> disconnectReason = 0;
  std::atomic<int> disconnectCount = 0;
  std::unique_ptr<RdpControl> control;
  auto connectStart = std::chrono::steady_clock::now();
  auto sinceConnect = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - connectStart).count(); };

  rdpThread->Invoke([&] {
    Log(std::format("RDP thread {} apartment {}", _options.Thread == L"dedicated" ? "dedicated" : "WinUI UI thread", ApartmentOfCurrentThread()));
    control = std::make_unique<RdpControl>(_options.Window, _options.ClassVersion, [&](const std::string& name, long argument) {
      Log(std::format("RDP event {}({}) at {:.2f} s", name, argument, sinceConnect()));
      if (name == "OnLoginComplete") loginComplete = true;
      if (name == "OnDisconnected")
      {
        disconnectReason = argument;
        disconnectCount++;
        disconnected = true;
      }
    });
    Log("mstscax.dll " + ModuleVersion(L"mstscax.dll"));
    Log("Host window after creation: " + control->WindowState());
    connectStart = std::chrono::steady_clock::now();
    control->Connect(user, domain, password, _options.Width, _options.Height);
  });
  SecureZeroMemory(password.data(), password.size() * sizeof(wchar_t));

  while (!loginComplete && !disconnected && sinceConnect() < 60) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  if (!loginComplete)
  {
    Log(std::format("SIGN-IN FAILED (disconnected={}, reason {})", disconnected.load(), disconnectReason.load()));
    rdpThread->Invoke([&] { control.reset(); });
    RegDeleteKeyValueW(HKEY_CURRENT_USER, startupKey, RunOnceValue);
    return;
  }
  Log(std::format("SIGNED IN after {:.2f} s", sinceConnect()));
  rdpThread->Invoke([&] {
    control->OnLoginComplete();
    Log("Host window after sign-in: " + control->WindowState());
  });

  auto childId = ExistingChildSession();
  if (!childId) throw std::runtime_error("Signed in, but WTSGetChildSessionId returns no session");
  Spike::WriteAll(outDir / L"expected_session.txt", std::to_string(*childId));
  Log(std::format("Child session {}, guard file written", *childId));

  //The visible WinUI window, as the real host would treat it
  if (_winUiWindow)
  {
    if (_options.WinUiWindow == L"minimized") ShowWindow(_winUiWindow, SW_SHOWMINNOACTIVE);
    else if (_options.WinUiWindow == L"hidden") ShowWindow(_winUiWindow, SW_HIDE);
  }

  //Wait for the program in the session
  std::atomic<bool> stopFrames = false;
  std::thread frameThread;
  bool started = false, ended = false, guardRefused = false;
  bool focusTaken = false, controlWasVisible = false;
  auto waitStart = std::chrono::steady_clock::now();
  auto lastStatus = waitStart;
  while (std::chrono::steady_clock::now() - waitStart < std::chrono::seconds(_options.TimeoutSeconds))
  {
    auto childLog = Spike::ReadAll(outDir / L"child.log");
    if (!started && childLog.find("START session=" + std::to_string(*childId)) != std::string::npos)
    {
      started = true;
      if (startupKey == RunKey) Log(std::format("Run entry removed: {}", RegDeleteKeyValueW(HKEY_CURRENT_USER, RunKey, RunOnceValue) == ERROR_SUCCESS));
      Log(std::format("Program in session started {:.1f} s after sign-in", std::chrono::duration<double>(std::chrono::steady_clock::now() - waitStart).count()));
    }
    if (childLog.find("GUARD:") != std::string::npos) guardRefused = true;
    if (childLog.find(" END") != std::string::npos) { ended = true; break; }

    if (_options.Frames && !frameThread.joinable() && std::filesystem::exists(outDir / L"frames.txt"))
    {
      frameThread = std::thread([&] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        try
        {
          FrameReader reader(_log, readerAdapter);
          reader.Run(outDir / L"frames.txt", std::chrono::seconds(_options.FrameSeconds), [&] { return stopFrames.load(); });
        }
        catch (const winrt::hresult_error& error) { Log(std::format("FRAMES ERROR 0x{:08X} {}", uint32_t(error.code()), winrt::to_string(error.message()))); }
        catch (const std::exception& error) { Log(std::format("FRAMES ERROR {}", error.what())); }
        Spike::WriteAll(outDir / L"frames_done.txt", "done");
        CoUninitialize();
      });
    }

    if (GetForegroundWindow() && [&] { DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid); return pid == GetCurrentProcessId(); }())
    {
      if (!focusTaken) Log("FOCUS: foreground is in this process: " + ForegroundDescription());
      focusTaken = true;
    }

    if (std::chrono::steady_clock::now() - lastStatus > std::chrono::seconds(10))
    {
      lastStatus = std::chrono::steady_clock::now();
      auto processes = SessionProcesses(*childId);
      static bool listed = false;
      if (!listed) Log(std::format("session {} processes: {}", *childId, processes));
      else Log(std::format("session {}: explorer {}", *childId, processes.find("explorer.exe") != std::string::npos ? "running" : "NOT running"));
      listed = true;
      rdpThread->Invoke([&] {
        if (IsWindowVisible(control->Window())) controlWasVisible = true;
        Log(std::format("status: connected={} disconnects={} window: {}", control->IsConnected(), disconnectCount.load(), control->WindowState()));
      });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }

  stopFrames = true;
  if (frameThread.joinable()) frameThread.join();

  rdpThread->Invoke([&] {
    if (IsWindowVisible(control->Window())) controlWasVisible = true;
    Log(std::format("final status: connected={} disconnects={} window: {}", control->IsConnected(), disconnectCount.load(), control->WindowState()));
  });
  Log(std::format("RESULT: program started={} ended={} guardRefused={} rdpDisconnectsBeforeLogoff={} hostWindowEverVisible={} focusTakenByHost={}",
    started, ended, guardRefused, disconnectCount.load(), controlWasVisible, focusTaken));

  //Clean up: log off explicitly, disconnect, remove RunOnce, check that no child session is left
  Log(std::format("WTSLogoffSession({}): {}", *childId, WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, *childId, TRUE) != 0));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  rdpThread->Invoke([&] {
    control->Disconnect();
  });
  std::this_thread::sleep_for(std::chrono::seconds(2));
  rdpThread->Invoke([&] { control.reset(); });
  rdpThread.reset();

  auto runOnceLeft = RegGetValueW(HKEY_CURRENT_USER, startupKey, RunOnceValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
  if (runOnceLeft) RegDeleteKeyValueW(HKEY_CURRENT_USER, startupKey, RunOnceValue);
  Log(std::format("Startup entry {}", runOnceLeft ? "was still there, removed" : "gone (consumed by Windows or removed after start)"));

  std::optional<uint32_t> left;
  for (int i = 0; i < 40 && (left = ExistingChildSession()); i++) std::this_thread::sleep_for(std::chrono::milliseconds(500));
  Log(left ? std::format("WARNING: child session {} still exists", *left) : "No child session left");
}



