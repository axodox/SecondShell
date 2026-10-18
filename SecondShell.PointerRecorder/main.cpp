//A test application for the session probe tests: a window over the whole screen which records the pointer input it
//receives, one JSON object per line, to the file given on its command line. The tests inject touch and pen input in
//the child session and compare it with the record. Not part of SecondShell itself.
#include <windows.h>
#include <shellapi.h>
#include <format>
#include <fstream>
#include <string>

namespace
{
  std::ofstream _record;

  const char* EventName(UINT message)
  {
    switch (message)
    {
    case WM_POINTERDOWN: return "down";
    case WM_POINTERUP: return "up";
    default: return "update";
    }
  }

  const char* TypeName(POINTER_INPUT_TYPE type)
  {
    switch (type)
    {
    case PT_TOUCH: return "touch";
    case PT_PEN: return "pen";
    case PT_MOUSE: return "mouse";
    default: return "other";
    }
  }

  const char* Boolean(bool value)
  {
    return value ? "true" : "false";
  }

  //{"event": "down", "type": "pen", "id": 1, "x": 10, "y": 20, "inContact": true, "canceled": false, "pressure": 512, ...}
  void Record(UINT message, WPARAM wParam)
  {
    auto id = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE type = PT_POINTER;
    POINTER_INFO info{};
    if (!GetPointerType(id, &type) || !GetPointerInfo(id, &info)) return;

    auto line = std::format(R"({{"event": "{}", "type": "{}", "id": {}, "x": {}, "y": {}, "inContact": {}, "canceled": {})",
      EventName(message), TypeName(type), id, info.ptPixelLocation.x, info.ptPixelLocation.y,
      Boolean(info.pointerFlags & POINTER_FLAG_INCONTACT), Boolean(info.pointerFlags & POINTER_FLAG_CANCELED));

    POINTER_PEN_INFO pen{};
    if (type == PT_PEN && GetPointerPenInfo(id, &pen))
    {
      line += std::format(R"(, "pressure": {}, "tiltX": {}, "tiltY": {}, "rotation": {}, "barrel": {}, "eraser": {}, "inverted": {})",
        pen.pressure, pen.tiltX, pen.tiltY, pen.rotation,
        Boolean(pen.penFlags & PEN_FLAG_BARREL), Boolean(pen.penFlags & PEN_FLAG_ERASER), Boolean(pen.penFlags & PEN_FLAG_INVERTED));
    }

    _record << line << "}\n";
    _record.flush();
  }

  LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
      Record(message, wParam);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wParam, lParam);
    }
  }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*previousInstance*/, PWSTR /*commandLine*/, int /*showCommand*/)
{
  //Positions are physical pixels, as the agent's are
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  auto argumentCount = 0;
  auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
  if (argumentCount < 2) return 1;

  _record.open(arguments[1]);
  LocalFree(arguments);
  if (!_record) return 1;

  WNDCLASSW windowClass{ .lpfnWndProc = WindowProc, .hInstance = instance, .hCursor = LoadCursorW(nullptr, IDC_ARROW), .hbrBackground = HBRUSH(GetStockObject(WHITE_BRUSH)), .lpszClassName = L"SecondShellPointerRecorder" };
  RegisterClassW(&windowClass);

  auto window = CreateWindowExW(0, windowClass.lpszClassName, L"SecondShell pointer recorder", WS_POPUP | WS_VISIBLE,
    0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, instance, nullptr);
  if (!window) return 1;

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0)
  {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return 0;
}
