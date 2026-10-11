#include "pch.h"
#include "MouseInjector.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace std;
using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace SecondShell::Input
{
  MouseInjector::MouseInjector(dependency_container* container) :
    _provider(container->resolve<InputInjectorProvider>())
  { }

  Point MouseInjector::Position() const
  {
    POINT position{};
    winrt::check_bool(GetCursorPos(&position));
    return { position.x, position.y };
  }

  Point MouseInjector::ScreenSize() const
  {
    return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
  }

  void MouseInjector::MoveTo(Point position)
  {
    //Absolute positions are normalized to 0..65535 over the primary monitor, the session's only one
    auto size = ScreenSize();
    auto x = int32_t(int64_t(position.X) * 65535 / max(size.X - 1, 1));
    auto y = int32_t(int64_t(position.Y) * 65535 / max(size.Y - 1, 1));
    Inject(InjectedInputMouseOptions::Move | InjectedInputMouseOptions::Absolute, 0, x, y);
  }

  void MouseInjector::Press(MouseButton button)
  {
    switch (button)
    {
    case MouseButton::Left:
      Inject(InjectedInputMouseOptions::LeftDown);
      break;
    case MouseButton::Right:
      Inject(InjectedInputMouseOptions::RightDown);
      break;
    case MouseButton::Middle:
      Inject(InjectedInputMouseOptions::MiddleDown);
      break;
    }
  }

  void MouseInjector::Release(MouseButton button)
  {
    switch (button)
    {
    case MouseButton::Left:
      Inject(InjectedInputMouseOptions::LeftUp);
      break;
    case MouseButton::Right:
      Inject(InjectedInputMouseOptions::RightUp);
      break;
    case MouseButton::Middle:
      Inject(InjectedInputMouseOptions::MiddleUp);
      break;
    }
  }

  void MouseInjector::Wheel(float notches, WheelAxis axis)
  {
    auto delta = int32_t(lround(notches * WHEEL_DELTA));
    Inject(axis == WheelAxis::Horizontal ? InjectedInputMouseOptions::HWheel : InjectedInputMouseOptions::Wheel, uint32_t(delta));
  }

  void MouseInjector::Inject(InjectedInputMouseOptions options, uint32_t data, int32_t x, int32_t y)
  {
    InjectedInputMouseInfo input;
    input.MouseOptions(options);
    input.MouseData(data);
    input.DeltaX(x);
    input.DeltaY(y);
    _provider->Injector().InjectMouseInput({ input });
  }
}
