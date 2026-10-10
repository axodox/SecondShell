//Spike 4, run inside the child session: do InputInjector touch and pen (and mouse buttons and wheels) reach a desktop
//app as pointer input? Also tests the Win32 fallback (CreateSyntheticPointerDevice + InjectSyntheticPointerInput).
//A test window records every pointer and mouse message it receives; each phase is summarized in child.log.
#include "SpikeCommon.h"
#include <windowsx.h>
#include <atomic>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Input.Preview.Injection.h>

using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace
{
  Spike::Logger* _log = nullptr;
  HWND _window = nullptr;
  std::mutex _mutex;
  std::string _phase = "idle";
  std::map<std::string, int> _counts;
  std::vector<std::string> _events;

  const char* PointerTypeName(POINTER_INPUT_TYPE type)
  {
    switch (type)
    {
    case PT_POINTER: return "pointer";
    case PT_TOUCH: return "touch";
    case PT_PEN: return "pen";
    case PT_MOUSE: return "mouse";
    case PT_TOUCHPAD: return "touchpad";
    default: return "?";
    }
  }

  void Record(const std::string& key, const std::string& detail, bool always)
  {
    std::lock_guard lock(_mutex);
    auto count = ++_counts[key];
    if (always || count == 1) _events.push_back(detail);
  }

  void OnPointer(const char* name, WPARAM wParam, bool always)
  {
    auto id = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE type = PT_POINTER;
    GetPointerType(id, &type);
    POINTER_INFO info{};
    GetPointerInfo(id, &info);
    std::string detail = std::format("{} {} id={} at ({},{}) flags=0x{:X}", name, PointerTypeName(type), id, info.ptPixelLocation.x, info.ptPixelLocation.y, uint32_t(info.pointerFlags));
    if (type == PT_PEN)
    {
      POINTER_PEN_INFO pen{};
      if (GetPointerPenInfo(id, &pen)) detail += std::format(" pressure={} tilt=({},{}) rotation={} penFlags=0x{:X}{}{}", pen.pressure, pen.tiltX, pen.tiltY, pen.rotation, pen.penFlags,
        (pen.penFlags & PEN_FLAG_BARREL) ? " BARREL" : "", (pen.penFlags & (PEN_FLAG_ERASER | PEN_FLAG_INVERTED)) ? " ERASER/INVERTED" : "");
    }
    else if (type == PT_TOUCH)
    {
      POINTER_TOUCH_INFO touch{};
      if (GetPointerTouchInfo(id, &touch)) detail += std::format(" pressure={} orientation={} contact=({},{})-({},{})", touch.pressure, touch.orientation, touch.rcContact.left, touch.rcContact.top, touch.rcContact.right, touch.rcContact.bottom);
    }
    Record(std::format("{} {}", name, PointerTypeName(type)), detail, always);
  }

  void OnMouse(const char* name, LPARAM lParam, bool screenCoordinates)
  {
    POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (!screenCoordinates) ClientToScreen(_window, &point);
    auto extra = GetMessageExtraInfo();
    const char* source = (extra & 0xFFFFFF00) == 0xFF515700 ? ((extra & 0x80) ? "promoted from touch" : "promoted from pen") : "mouse";
    Record(std::format("{} ({})", name, source), std::format("{} ({}) at screen ({},{})", name, source, point.x, point.y), true);
  }

  LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case WM_POINTERDOWN: OnPointer("WM_POINTERDOWN", wParam, true); break;
    case WM_POINTERUP: OnPointer("WM_POINTERUP", wParam, true); break;
    case WM_POINTERUPDATE: OnPointer("WM_POINTERUPDATE", wParam, false); break;
    case WM_POINTERENTER: OnPointer("WM_POINTERENTER", wParam, false); break;
    case WM_POINTERLEAVE: OnPointer("WM_POINTERLEAVE", wParam, false); break;
    case WM_LBUTTONDOWN: OnMouse("WM_LBUTTONDOWN", lParam, false); break;
    case WM_LBUTTONUP: OnMouse("WM_LBUTTONUP", lParam, false); break;
    case WM_RBUTTONDOWN: OnMouse("WM_RBUTTONDOWN", lParam, false); break;
    case WM_RBUTTONUP: OnMouse("WM_RBUTTONUP", lParam, false); break;
    case WM_MBUTTONDOWN: OnMouse("WM_MBUTTONDOWN", lParam, false); break;
    case WM_MBUTTONUP: OnMouse("WM_MBUTTONUP", lParam, false); break;
    case WM_XBUTTONDOWN: OnMouse(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? "WM_XBUTTONDOWN(X1)" : "WM_XBUTTONDOWN(X2)", lParam, false); break;
    case WM_XBUTTONUP: OnMouse("WM_XBUTTONUP", lParam, false); break;
    case WM_MOUSEWHEEL: Record("WM_MOUSEWHEEL", std::format("WM_MOUSEWHEEL delta={}", GET_WHEEL_DELTA_WPARAM(wParam)), true); break;
    case WM_MOUSEHWHEEL: Record("WM_MOUSEHWHEEL", std::format("WM_MOUSEHWHEEL delta={}", GET_WHEEL_DELTA_WPARAM(wParam)), true); break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    //DefWindowProc turns unhandled touch and pen pointer messages into (promoted) mouse messages
    return DefWindowProcW(window, message, wParam, lParam);
  }

  void BeginPhase(const std::string& name)
  {
    std::lock_guard lock(_mutex);
    _phase = name;
    _counts.clear();
    _events.clear();
    (*_log)("=== PHASE " + name);
  }

  void EndPhase()
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    std::lock_guard lock(_mutex);
    for (auto& event : _events) (*_log)("  " + event);
    std::string summary;
    for (auto& [key, count] : _counts) summary += std::format("{} x{}; ", key, count);
    (*_log)(std::format("  SUMMARY {}: {}", _phase, summary.empty() ? "NOTHING RECEIVED" : summary));
  }

  void Pause(int milliseconds = 30) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

  //InputInjector helpers
  InjectedInputPointerInfo PointerInfo(uint32_t id, InjectedInputPointerOptions options, POINT point)
  {
    InjectedInputPointerInfo info{};
    info.PointerId = id;
    info.PointerOptions = options;
    info.PixelLocation = { point.x, point.y };
    return info;
  }

  bool _absoluteContact = false;

  InjectedInputTouchInfo Touch(uint32_t id, InjectedInputPointerOptions options, POINT point)
  {
    InjectedInputTouchInfo touch;
    touch.PointerInfo(PointerInfo(id, options, point));
    //InjectedInputRectangle is Left, Top, Bottom, Right: offsets around the point, or (variant) absolute coordinates
    if (_absoluteContact) touch.Contact({ point.x - 4, point.y - 4, point.y + 4, point.x + 4 });
    else touch.Contact({ 4, 4, 4, 4 });
    touch.Orientation(90);
    touch.Pressure(0.8);
    touch.TouchParameters(InjectedInputTouchParameters::Contact | InjectedInputTouchParameters::Orientation | InjectedInputTouchParameters::Pressure);
    return touch;
  }

  InjectedInputPenInfo Pen(InjectedInputPointerOptions options, POINT point, double pressure, InjectedInputPenButtons buttons = InjectedInputPenButtons::None)
  {
    InjectedInputPenInfo pen;
    pen.PointerInfo(PointerInfo(1, options, point));
    pen.Pressure(pressure);
    pen.TiltX(15);
    pen.TiltY(-10);
    pen.Rotation(30);
    pen.PenButtons(buttons);
    pen.PenParameters(InjectedInputPenParameters::Pressure | InjectedInputPenParameters::TiltX | InjectedInputPenParameters::TiltY | InjectedInputPenParameters::Rotation);
    return pen;
  }

  void Mouse(const InputInjector& injector, InjectedInputMouseOptions options, uint32_t data = 0, POINT* moveTo = nullptr)
  {
    InjectedInputMouseInfo mouse;
    mouse.MouseOptions(options);
    mouse.MouseData(data);
    if (moveTo)
    {
      mouse.DeltaX(moveTo->x * 65535 / (GetSystemMetrics(SM_CXSCREEN) - 1));
      mouse.DeltaY(moveTo->y * 65535 / (GetSystemMetrics(SM_CYSCREEN) - 1));
    }
    injector.InjectMouseInput({ mouse });
    Pause(80);
  }

  using Options = InjectedInputPointerOptions;

  void RunInputInjector(POINT center)
  {
    auto injector = InputInjector::TryCreate();
    (*_log)(std::format("InputInjector.TryCreate: {}", injector ? "OK" : "null"));
    if (!injector) return;

    //Mouse buttons and wheels
    BeginPhase("InputInjector mouse buttons and wheels");
    Mouse(injector, InjectedInputMouseOptions::Move | InjectedInputMouseOptions::Absolute, 0, &center);
    Mouse(injector, InjectedInputMouseOptions::LeftDown);
    Mouse(injector, InjectedInputMouseOptions::LeftUp);
    Mouse(injector, InjectedInputMouseOptions::RightDown);
    Mouse(injector, InjectedInputMouseOptions::RightUp);
    Mouse(injector, InjectedInputMouseOptions::MiddleDown);
    Mouse(injector, InjectedInputMouseOptions::MiddleUp);
    Mouse(injector, InjectedInputMouseOptions::XDown, XBUTTON1);
    Mouse(injector, InjectedInputMouseOptions::XUp, XBUTTON1);
    Mouse(injector, InjectedInputMouseOptions::Wheel, 120);
    Mouse(injector, InjectedInputMouseOptions::HWheel, 120);
    EndPhase();

    //Move the mouse away so it does not interfere with the following phases
    POINT corner{ 5, 5 };
    Mouse(injector, InjectedInputMouseOptions::Move | InjectedInputMouseOptions::Absolute, 0, &corner);

    //Touch
    injector.InitializeTouchInjection(InjectedInputVisualizationMode::Default);
    BeginPhase("InputInjector touch tap");
    injector.InjectTouchInput({ Touch(0, Options::PointerDown | Options::InRange | Options::InContact | Options::New, center) });
    Pause(60);
    injector.InjectTouchInput({ Touch(0, Options::PointerUp, center) });
    EndPhase();

    BeginPhase("InputInjector touch drag");
    POINT point = { center.x - 200, center.y };
    injector.InjectTouchInput({ Touch(0, Options::PointerDown | Options::InRange | Options::InContact | Options::New, point) });
    for (int i = 0; i < 20; i++)
    {
      Pause(16);
      point.x += 20;
      injector.InjectTouchInput({ Touch(0, Options::Update | Options::InRange | Options::InContact, point) });
    }
    injector.InjectTouchInput({ Touch(0, Options::PointerUp, point) });
    EndPhase();

    BeginPhase("InputInjector two-finger touch");
    POINT left{ center.x - 100, center.y }, right{ center.x + 100, center.y };
    injector.InjectTouchInput({ Touch(0, Options::PointerDown | Options::InRange | Options::InContact | Options::New, left), Touch(1, Options::PointerDown | Options::InRange | Options::InContact | Options::New, right) });
    for (int i = 0; i < 10; i++)
    {
      Pause(16);
      left.x -= 10;
      right.x += 10;
      injector.InjectTouchInput({ Touch(0, Options::Update | Options::InRange | Options::InContact, left), Touch(1, Options::Update | Options::InRange | Options::InContact, right) });
    }
    injector.InjectTouchInput({ Touch(0, Options::PointerUp, left), Touch(1, Options::PointerUp, right) });
    EndPhase();

    BeginPhase("InputInjector touch tap, contact rectangle in absolute coordinates");
    _absoluteContact = true;
    injector.InjectTouchInput({ Touch(0, Options::PointerDown | Options::InRange | Options::InContact | Options::New, center) });
    Pause(60);
    injector.InjectTouchInput({ Touch(0, Options::PointerUp, center) });
    _absoluteContact = false;
    EndPhase();
    injector.UninitializeTouchInjection();

    //Pen
    injector.InitializePenInjection(InjectedInputVisualizationMode::Default);
    BeginPhase("InputInjector pen hover, stroke with pressure and tilt");
    point = { center.x - 150, center.y + 50 };
    injector.InjectPenInput(Pen(Options::InRange | Options::New, point, 0));
    for (int i = 0; i < 5; i++) { Pause(16); point.x += 5; injector.InjectPenInput(Pen(Options::InRange | Options::Update, point, 0)); }
    injector.InjectPenInput(Pen(Options::InRange | Options::InContact | Options::PointerDown, point, 0.3));
    for (int i = 0; i < 15; i++) { Pause(16); point.x += 15; injector.InjectPenInput(Pen(Options::InRange | Options::InContact | Options::Update, point, 0.3 + i * 0.04)); }
    injector.InjectPenInput(Pen(Options::InRange | Options::PointerUp, point, 0));
    Pause(16);
    injector.InjectPenInput(Pen(Options::Update, point, 0));
    EndPhase();

    BeginPhase("InputInjector pen barrel button and eraser");
    point = { center.x, center.y - 100 };
    injector.InjectPenInput(Pen(Options::InRange | Options::InContact | Options::PointerDown | Options::New, point, 0.5, InjectedInputPenButtons::Barrel));
    Pause(60);
    injector.InjectPenInput(Pen(Options::InRange | Options::PointerUp, point, 0, InjectedInputPenButtons::Barrel));
    Pause(60);
    point.y += 50;
    injector.InjectPenInput(Pen(Options::InRange | Options::InContact | Options::PointerDown, point, 0.5, InjectedInputPenButtons::Eraser));
    Pause(60);
    injector.InjectPenInput(Pen(Options::InRange | Options::PointerUp, point, 0, InjectedInputPenButtons::Eraser));
    Pause(16);
    injector.InjectPenInput(Pen(Options::Update, point, 0));
    EndPhase();

    BeginPhase("InputInjector pen eraser as Inverted + Eraser");
    point = { center.x + 100, center.y - 100 };
    injector.InjectPenInput(Pen(Options::InRange | Options::New, point, 0, InjectedInputPenButtons::Inverted));
    Pause(30);
    injector.InjectPenInput(Pen(Options::InRange | Options::InContact | Options::PointerDown, point, 0.5, InjectedInputPenButtons::Inverted | InjectedInputPenButtons::Eraser));
    for (int i = 0; i < 5; i++) { Pause(16); point.x += 10; injector.InjectPenInput(Pen(Options::InRange | Options::InContact | Options::Update, point, 0.5, InjectedInputPenButtons::Inverted | InjectedInputPenButtons::Eraser)); }
    injector.InjectPenInput(Pen(Options::InRange | Options::PointerUp, point, 0, InjectedInputPenButtons::Inverted));
    Pause(16);
    injector.InjectPenInput(Pen(Options::Update, point, 0));
    EndPhase();
    injector.UninitializePenInjection();
  }

  //Win32 fallback: synthetic pointer devices
  void RunSynthetic(POINT center)
  {
    BeginPhase("Synthetic pointer touch tap and drag");
    auto touchDevice = CreateSyntheticPointerDevice(PT_TOUCH, 2, POINTER_FEEDBACK_DEFAULT);
    (*_log)(std::format("CreateSyntheticPointerDevice(PT_TOUCH): {}", touchDevice ? "OK" : std::to_string(GetLastError())));
    if (touchDevice)
    {
      auto touch = [&](POINT point, POINTER_FLAGS flags) {
        POINTER_TYPE_INFO info{};
        info.type = PT_TOUCH;
        auto& t = info.touchInfo;
        t.pointerInfo.pointerType = PT_TOUCH;
        t.pointerInfo.pointerId = 0;
        t.pointerInfo.ptPixelLocation = point;
        t.pointerInfo.pointerFlags = flags;
        t.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
        t.rcContact = { point.x - 4, point.y - 4, point.x + 4, point.y + 4 };
        t.orientation = 90;
        t.pressure = 32000;
        if (!InjectSyntheticPointerInput(touchDevice, &info, 1)) (*_log)(std::format("InjectSyntheticPointerInput failed: {}", GetLastError()));
      };
      touch(center, POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT);
      Pause(60);
      touch(center, POINTER_FLAG_UP);
      Pause(100);
      POINT point{ center.x - 200, center.y + 100 };
      touch(point, POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT);
      for (int i = 0; i < 20; i++) { Pause(16); point.x += 20; touch(point, POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT); }
      touch(point, POINTER_FLAG_UP);
      DestroySyntheticPointerDevice(touchDevice);
    }
    EndPhase();

    BeginPhase("Synthetic pointer pen stroke with pressure and tilt");
    auto penDevice = CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_DEFAULT);
    (*_log)(std::format("CreateSyntheticPointerDevice(PT_PEN): {}", penDevice ? "OK" : std::to_string(GetLastError())));
    if (penDevice)
    {
      auto pen = [&](POINT point, POINTER_FLAGS flags, UINT32 pressure) {
        POINTER_TYPE_INFO info{};
        info.type = PT_PEN;
        auto& p = info.penInfo;
        p.pointerInfo.pointerType = PT_PEN;
        p.pointerInfo.pointerId = 0;
        p.pointerInfo.ptPixelLocation = point;
        p.pointerInfo.pointerFlags = flags;
        p.penMask = PEN_MASK_PRESSURE | PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
        p.pressure = pressure;
        p.tiltX = 15;
        p.tiltY = -10;
        if (!InjectSyntheticPointerInput(penDevice, &info, 1)) (*_log)(std::format("InjectSyntheticPointerInput failed: {}", GetLastError()));
      };
      POINT point{ center.x - 150, center.y - 50 };
      pen(point, POINTER_FLAG_INRANGE | POINTER_FLAG_UPDATE, 0);
      Pause(16);
      pen(point, POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT, 300);
      for (int i = 0; i < 15; i++) { Pause(16); point.x += 15; pen(point, POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT, 300 + i * 40); }
      pen(point, POINTER_FLAG_UP | POINTER_FLAG_INRANGE, 0);
      Pause(16);
      pen(point, POINTER_FLAG_UPDATE, 0);
      DestroySyntheticPointerDevice(penDevice);
    }
    EndPhase();
  }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
  Spike::Logger log(Spike::OutDir() / L"child.log");
  _log = &log;
  Spike::SessionGuard(log);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  winrt::init_apartment(winrt::apartment_type::multi_threaded);

  WNDCLASSW windowClass{ .lpfnWndProc = WindowProc, .hInstance = instance, .hCursor = LoadCursor(nullptr, IDC_ARROW), .hbrBackground = HBRUSH(GetStockObject(WHITE_BRUSH)), .lpszClassName = L"SpikeTouchPen" };
  RegisterClassW(&windowClass);
  _window = CreateWindowExW(0, windowClass.lpszClassName, L"Touch and pen spike", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 1200, 800, nullptr, nullptr, instance, nullptr);
  SetForegroundWindow(_window);

  RECT client;
  GetClientRect(_window, &client);
  POINT center{ client.right / 2, client.bottom / 2 };
  ClientToScreen(_window, &center);
  log(std::format("Window client center at screen ({},{}), DPI {}, foreground {}, screen {}x{}", center.x, center.y, GetDpiForWindow(_window), GetForegroundWindow() == _window, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));

  std::thread worker([center] {
    try
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      RunInputInjector(center);
      RunSynthetic(center);
    }
    catch (const winrt::hresult_error& error) { (*_log)(std::format("ERROR 0x{:08X} {}", uint32_t(error.code()), winrt::to_string(error.message()))); }
    catch (const std::exception& error) { (*_log)(std::format("ERROR {}", error.what())); }
    PostMessageW(_window, WM_CLOSE, 0, 0);
  });

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0)
  {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  worker.join();
  log("END");
  return 0;
}


