//Spike 1 payload, run inside the child session: checks that the session keeps working while the host's RDP control is hidden.
//Every 2 s: repaints a marker window with a new colour, captures the screen with GDI and checks the colour (fresh frames),
//then moves the mouse with InputInjector and checks the cursor position (input).
#include "SpikeCommon.h"
#include <atomic>
#include <random>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Input.Preview.Injection.h>

using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace
{
  std::atomic<COLORREF> _color = RGB(0, 0, 0);

  LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    if (message == WM_PAINT)
    {
      PAINTSTRUCT paint;
      auto dc = BeginPaint(window, &paint);
      auto brush = CreateSolidBrush(_color);
      RECT rect;
      GetClientRect(window, &rect);
      FillRect(dc, &rect, brush);
      DeleteObject(brush);
      EndPaint(window, &paint);
      return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
  }

  COLORREF CaptureScreenPixel(POINT point)
  {
    auto screen = GetDC(nullptr);
    auto memory = CreateCompatibleDC(screen);
    auto width = GetSystemMetrics(SM_CXSCREEN), height = GetSystemMetrics(SM_CYSCREEN);
    auto bitmap = CreateCompatibleBitmap(screen, width, height);
    auto old = SelectObject(memory, bitmap);
    COLORREF result = CLR_INVALID;
    if (BitBlt(memory, 0, 0, width, height, screen, 0, 0, SRCCOPY)) result = GetPixel(memory, point.x, point.y);
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return result;
  }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
  Spike::Logger log(Spike::OutDir() / L"child.log");
  Spike::SessionGuard(log);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  winrt::init_apartment();

  int argc;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  auto seconds = std::stoi(Spike::ArgValue(argc, argv, L"--seconds", L"60"));

  WNDCLASSW windowClass{ .lpfnWndProc = WindowProc, .hInstance = instance, .lpszClassName = L"SpikeSessionCheck" };
  RegisterClassW(&windowClass);
  auto window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, windowClass.lpszClassName, L"SessionCheck", WS_POPUP | WS_VISIBLE, 40, 40, 200, 120, nullptr, nullptr, instance, nullptr);

  log(std::format("Screen {}x{}, running {} s", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), seconds));

  std::thread worker([&] {
    auto injector = InputInjector::TryCreate();
    log(std::format("InputInjector.TryCreate: {}", injector ? "OK" : "null"));

    std::mt19937 random(42);
    int ticks = 0, freshOk = 0, inputOk = 0;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(seconds))
    {
      ticks++;
      COLORREF color = RGB(ticks * 37 & 0xFF, ticks * 91 & 0xFF, 0x5A);
      _color = color;
      RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      auto captured = CaptureScreenPixel({ 140, 100 });
      bool fresh = captured == color;
      if (fresh) freshOk++;

      bool moved = false;
      POINT target{ 300 + int(random() % 600), 250 + int(random() % 300) };
      if (injector)
      {
        InjectedInputMouseInfo mouse;
        mouse.DeltaX(target.x * 65535 / (GetSystemMetrics(SM_CXSCREEN) - 1));
        mouse.DeltaY(target.y * 65535 / (GetSystemMetrics(SM_CYSCREEN) - 1));
        mouse.MouseOptions(InjectedInputMouseOptions::Move | InjectedInputMouseOptions::Absolute);
        injector.InjectMouseInput({ mouse });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        POINT cursor;
        GetCursorPos(&cursor);
        moved = abs(cursor.x - target.x) <= 1 && abs(cursor.y - target.y) <= 1;
        if (moved) inputOk++;
        log(std::format("tick {}: capture {} (expected {:06X}, got {:06X}); mouse target ({},{}) cursor ({},{}) {}",
          ticks, fresh ? "FRESH" : "STALE", color, captured, target.x, target.y, cursor.x, cursor.y, moved ? "OK" : "MISMATCH"));
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1550));
    }

    log(std::format("SUMMARY ticks={} fresh={} input={}", ticks, freshOk, inputOk));
    log("END");
    PostMessageW(window, WM_CLOSE, 0, 0);
  });

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0)
  {
    if (message.message == WM_CLOSE || (message.hwnd == window && message.message == WM_DESTROY)) break;
    TranslateMessage(&message);
    DispatchMessageW(&message);
    if (!IsWindow(window)) break;
  }
  worker.join();
  return 0;
}
