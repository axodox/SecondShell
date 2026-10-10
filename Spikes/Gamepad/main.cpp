//Spike 3: does an InputInjector gamepad reach Windows.Gaming.Input, XInput and GameInput?
//  Gamepad.exe --inject [--seconds N]   in the child session (session guard): injects a gamepad and reads it back
//  Gamepad.exe --observe --seconds N    in the user's session, read only: logs which gamepads and readings it sees,
//                                       to check that the injected gamepad does not leak out of the child session
#include "SpikeCommon.h"
#include <atomic>
#include <set>
#include <GameInput.h>
#include <Xinput.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.UI.Input.Preview.Injection.h>

#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "gameinput.lib")

using namespace winrt::Windows::Gaming::Input;
using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace
{
  Spike::Logger* _log = nullptr;
  IGameInput* _gameInput = nullptr;

  std::string WgiDevices()
  {
    std::string result = std::format("Gamepads={}", Gamepad::Gamepads().Size());
    for (auto&& controller : RawGameController::RawGameControllers())
      result += std::format(" [raw '{}' vid {:04X} pid {:04X}]", winrt::to_string(controller.DisplayName()), controller.HardwareVendorId(), controller.HardwareProductId());
    return result;
  }

  std::string WgiReadings()
  {
    std::string result;
    int index = 0;
    for (auto&& gamepad : Gamepad::Gamepads())
    {
      auto reading = gamepad.GetCurrentReading();
      result += std::format("[{}: buttons 0x{:X} LX {:.2f} LY {:.2f} RT {:.2f}] ", index++, uint32_t(reading.Buttons), reading.LeftThumbstickX, reading.LeftThumbstickY, reading.RightTrigger);
    }
    return result.empty() ? "none" : result;
  }

  std::string XInputStates()
  {
    std::string result;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
    {
      XINPUT_STATE state{};
      if (XInputGetState(i, &state) == ERROR_SUCCESS)
        result += std::format("[slot {}: buttons 0x{:04X} LX {} LY {} RT {}] ", i, state.Gamepad.wButtons, state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, state.Gamepad.bRightTrigger);
    }
    return result.empty() ? "none" : result;
  }

  std::string GameInputState()
  {
    if (!_gameInput) return "unavailable";
    IGameInputReading* reading = nullptr;
    if (FAILED(_gameInput->GetCurrentReading(GameInputKindGamepad, nullptr, &reading)) || !reading) return "none";
    GameInputGamepadState state{};
    reading->GetGamepadState(&state);
    reading->Release();
    return std::format("[buttons 0x{:X} LX {:.2f} LY {:.2f} RT {:.2f}]", uint32_t(state.buttons), state.leftThumbstickX, state.leftThumbstickY, state.rightTrigger);
  }

  void CALLBACK OnGameInputDevice(GameInputCallbackToken, void*, IGameInputDevice* device, uint64_t, GameInputDeviceStatus current, GameInputDeviceStatus previous)
  {
    auto info = device->GetDeviceInfo();
    (*_log)(std::format("GameInput device {} ('{}' vid {:04X} pid {:04X}): status 0x{:X} -> 0x{:X}", (current & GameInputDeviceConnected) ? "CONNECTED" : "disconnected",
      info->displayName ? info->displayName->data : "", info->vendorId, info->productId, uint32_t(previous), uint32_t(current)));
  }

  void Snapshot(const char* label)
  {
    (*_log)(std::format("[{}] WGI: {} | readings {} | XInput: {} | GameInput: {}", label, WgiDevices(), WgiReadings(), XInputStates(), GameInputState()));
  }

  void SetUpReaders(Spike::Logger& log)
  {
    _log = &log;
    Gamepad::GamepadAdded([&](auto&&, const Gamepad&) { log("WGI GamepadAdded"); });
    Gamepad::GamepadRemoved([&](auto&&, const Gamepad&) { log("WGI GamepadRemoved"); });
    RawGameController::RawGameControllerAdded([&](auto&&, const RawGameController& controller) { log(std::format("WGI RawGameControllerAdded '{}'", winrt::to_string(controller.DisplayName()))); });
    RawGameController::RawGameControllerRemoved([&](auto&&, const RawGameController&) { log("WGI RawGameControllerRemoved"); });

    auto result = GameInputCreate(&_gameInput);
    log(std::format("GameInputCreate: 0x{:08X}", uint32_t(result)));
    if (_gameInput)
    {
      GameInputCallbackToken token;
      _gameInput->RegisterDeviceCallback(nullptr, GameInputKindGamepad, GameInputDeviceAnyStatus, GameInputBlockingEnumeration, nullptr, OnGameInputDevice, &token);
    }
  }

  //Polls all three APIs until the deadline; logs whenever what an API reports changes.
  void Watch(std::chrono::milliseconds duration, std::string& lastWgi, std::string& lastXInput, std::string& lastGameInput)
  {
    auto end = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < end)
    {
      auto wgi = WgiReadings(), xinput = XInputStates(), gameInput = GameInputState();
      if (wgi != lastWgi) (*_log)("WGI reading: " + (lastWgi = wgi));
      if (xinput != lastXInput) (*_log)("XInput state: " + (lastXInput = xinput));
      if (gameInput != lastGameInput) (*_log)("GameInput reading: " + (lastGameInput = gameInput));
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  void Inject(const InputInjector& injector, GamepadButtons buttons, double leftX, double leftY, double rightTrigger)
  {
    InjectedInputGamepadInfo info;
    info.Buttons(buttons);
    info.LeftThumbstickX(leftX);
    info.LeftThumbstickY(leftY);
    info.RightTrigger(rightTrigger);
    injector.InjectGamepadInput(info);
  }

  LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    return DefWindowProcW(window, message, wParam, lParam);
  }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
  int argc;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  auto observe = Spike::HasArg(argc, argv, L"--observe");
  auto seconds = std::stoi(Spike::ArgValue(argc, argv, L"--seconds", L"30"));

  auto withWindow = Spike::HasArg(argc, argv, L"--window");
  auto logName = Spike::ArgValue(argc, argv, L"--log", observe ? L"observer.log" : L"child.log");
  Spike::Logger log(Spike::OutDir() / logName);
  if (!observe) Spike::SessionGuard(log);
  else log(std::format("OBSERVER START session={} pid={} (read only, no injection){}", Spike::CurrentSessionId(), GetCurrentProcessId(), withWindow ? ", with a foreground window" : ""));
  winrt::init_apartment(winrt::apartment_type::multi_threaded);

  try
  {
    SetUpReaders(log);
    std::string wgi, xinput, gameInput;

    if (observe)
    {
      if (withWindow)
      {
        WNDCLASSW observerClass{ .lpfnWndProc = WindowProc, .hInstance = instance, .hbrBackground = HBRUSH(GetStockObject(DKGRAY_BRUSH)), .lpszClassName = L"SpikeGamepadReader" };
        RegisterClassW(&observerClass);
        auto readerWindow = CreateWindowExW(0, observerClass.lpszClassName, L"Gamepad reader", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 150, 150, 640, 400, nullptr, nullptr, instance, nullptr);
        SetForegroundWindow(readerWindow);
        MSG pending;
        while (PeekMessageW(&pending, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&pending);
        log(std::format("Reader window foreground: {}", GetForegroundWindow() == readerWindow));
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
      Snapshot("baseline");
      Watch(std::chrono::seconds(seconds), wgi, xinput, gameInput);
      Snapshot("end");
      log("OBSERVER END");
      return 0;
    }

    //A separate reader process in this session (the injecting process might not see its own gamepad)
    auto separateReader = Spike::HasArg(argc, argv, L"--separate-reader");
    if (separateReader)
    {
      wchar_t self[MAX_PATH];
      GetModuleFileNameW(nullptr, self, MAX_PATH);
      std::wstring readerCommand = std::format(L"\"{}\" --observe --window --seconds 25 --log reader.log", self);
      STARTUPINFOW startup{ .cb = sizeof(startup) };
      PROCESS_INFORMATION reader{};
      AllowSetForegroundWindow(ASFW_ANY);
      if (CreateProcessW(nullptr, readerCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &reader)) { CloseHandle(reader.hThread); CloseHandle(reader.hProcess); log("Reader process started"); }
      else log(std::format("Starting the reader failed: {}", GetLastError()));
      std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    //A foreground window, as a game would have
    WNDCLASSW windowClass{ .lpfnWndProc = WindowProc, .hInstance = instance, .hbrBackground = HBRUSH(GetStockObject(GRAY_BRUSH)), .lpszClassName = L"SpikeGamepad" };
    RegisterClassW(&windowClass);
    auto window = CreateWindowExW(0, windowClass.lpszClassName, L"Gamepad spike", WS_OVERLAPPEDWINDOW | (separateReader ? 0 : WS_VISIBLE), 100, 100, 640, 400, nullptr, nullptr, instance, nullptr);
    if (!separateReader) SetForegroundWindow(window);
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
    log(std::format("Window foreground: {}", GetForegroundWindow() == window));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    Snapshot("before injection");

    auto injector = InputInjector::TryCreate();
    log(std::format("InputInjector.TryCreate: {}", injector ? "OK" : "null"));
    if (!injector) throw std::runtime_error("no injector");

    injector.InitializeGamepadInjection();
    log("InitializeGamepadInjection done");
    Watch(std::chrono::seconds(3), wgi, xinput, gameInput);
    Snapshot("after InitializeGamepadInjection");

    struct Step { const char* Name; GamepadButtons Buttons; double LeftX, LeftY, RightTrigger; };
    Step steps[] = {
      { "neutral", GamepadButtons::None, 0, 0, 0 },
      { "A + LX 0.5 + RT 1.0", GamepadButtons::A, 0.5, 0, 1.0 },
      { "B + LY -0.75", GamepadButtons::B, 0, -0.75, 0 },
      { "neutral", GamepadButtons::None, 0, 0, 0 },
    };
    for (auto& step : steps)
    {
      log(std::format("INJECT {}", step.Name));
      for (int i = 0; i < 20; i++)
      {
        Inject(injector, step.Buttons, step.LeftX, step.LeftY, step.RightTrigger);
        Watch(std::chrono::milliseconds(100), wgi, xinput, gameInput);
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
      }
      Snapshot(step.Name);
    }

    injector.UninitializeGamepadInjection();
    log("UninitializeGamepadInjection done");
    Watch(std::chrono::seconds(3), wgi, xinput, gameInput);
    Snapshot("after UninitializeGamepadInjection");
    DestroyWindow(window);
  }
  catch (const winrt::hresult_error& error)
  {
    log(std::format("ERROR 0x{:08X} {}", uint32_t(error.code()), winrt::to_string(error.message())));
  }
  catch (const std::exception& error)
  {
    log(std::format("ERROR {}", error.what()));
  }

  log("END");
  return 0;
}

