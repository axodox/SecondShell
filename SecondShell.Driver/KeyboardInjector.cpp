#include "pch.h"
#include "KeyboardInjector.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace SecondShell::Input
{
  namespace
  {
    //Keys whose scan codes have the E0 prefix: without the flag, e.g. the arrows would arrive as the numpad's
    bool IsExtendedKey(uint16_t virtualKey)
    {
      switch (virtualKey)
      {
      case VK_RMENU:
      case VK_RCONTROL:
      case VK_INSERT:
      case VK_DELETE:
      case VK_HOME:
      case VK_END:
      case VK_PRIOR:
      case VK_NEXT:
      case VK_LEFT:
      case VK_RIGHT:
      case VK_UP:
      case VK_DOWN:
      case VK_NUMLOCK:
      case VK_SNAPSHOT:
      case VK_DIVIDE:
      case VK_LWIN:
      case VK_RWIN:
      case VK_APPS:
        return true;
      default:
        return false;
      }
    }
  }

  KeyboardInjector::KeyboardInjector(dependency_container* container) :
    _provider(container->resolve<InputInjectorProvider>())
  { }

  void KeyboardInjector::Press(uint16_t virtualKey)
  {
    InjectKey(virtualKey, false);
  }

  void KeyboardInjector::Release(uint16_t virtualKey)
  {
    InjectKey(virtualKey, true);
  }

  void KeyboardInjector::TypeCharacter(char32_t character)
  {
    //Characters outside the basic plane are sent as their UTF-16 surrogate pair
    if (character > 0xFFFF)
    {
      character -= 0x10000;
      InjectUnicode(wchar_t(0xD800 + (character >> 10)));
      InjectUnicode(wchar_t(0xDC00 + (character & 0x3FF)));
    }
    else
    {
      InjectUnicode(wchar_t(character));
    }
  }

  void KeyboardInjector::InjectKey(uint16_t virtualKey, bool isUp)
  {
    auto options = InjectedInputKeyOptions::None;
    if (IsExtendedKey(virtualKey)) options |= InjectedInputKeyOptions::ExtendedKey;
    if (isUp) options |= InjectedInputKeyOptions::KeyUp;

    InjectedInputKeyboardInfo input;
    input.VirtualKey(virtualKey);
    input.ScanCode(uint16_t(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC)));
    input.KeyOptions(options);
    _provider->Injector().InjectKeyboardInput({ input });
  }

  void KeyboardInjector::InjectUnicode(wchar_t codeUnit)
  {
    InjectedInputKeyboardInfo down;
    down.ScanCode(uint16_t(codeUnit));
    down.KeyOptions(InjectedInputKeyOptions::Unicode);

    InjectedInputKeyboardInfo up;
    up.ScanCode(uint16_t(codeUnit));
    up.KeyOptions(InjectedInputKeyOptions::Unicode | InjectedInputKeyOptions::KeyUp);

    _provider->Injector().InjectKeyboardInput({ down, up });
  }
}
