#include "pch.h"
#include "PenInjector.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;
using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace SecondShell::Input
{
  const milliseconds _repeatAfter{ 40 };

  namespace
  {
    //The pointer flags for going from one state to the next
    InjectedInputPointerOptions OptionsOf(const PenState& previous, const PenState& next)
    {
      using enum InjectedInputPointerOptions;
      if (!next.IsInRange) return Update;

      auto options = InRange;
      if (!previous.IsInRange) options |= New;
      if (next.IsInContact) options |= InContact;

      if (next.IsInContact && !previous.IsInContact) options |= PointerDown;
      else if (!next.IsInContact && previous.IsInContact) options |= PointerUp;
      else options |= Update;
      return options;
    }

    //The eraser end shows as inverted while hovering, and as inverted and erasing in contact (Findings.md 2.14)
    InjectedInputPenButtons ButtonsOf(const PenState& state)
    {
      using enum InjectedInputPenButtons;
      auto buttons = None;
      if (state.IsBarrelPressed) buttons |= Barrel;
      if (state.IsEraser) buttons |= state.IsInContact ? Inverted | Eraser : Inverted;
      return buttons;
    }
  }

  PenInjector::PenInjector(dependency_container* container) :
    _provider(container->resolve<InputInjectorProvider>()),
    _heartbeat([this] { RepeatIfIdle(); })
  { }

  PenInjector::~PenInjector()
  {
    lock_guard lock{ _mutex };
    _state = {};
    if (_isInitialized) _provider->Injector().UninitializePenInjection();
  }

  void PenInjector::Inject(const PenState& state)
  {
    lock_guard lock{ _mutex };
    if (!_isInitialized)
    {
      _provider->Injector().InitializePenInjection(InjectedInputVisualizationMode::Default);
      _isInitialized = true;
    }

    //Out of range already, there is nothing to tell
    if (!state.IsInRange && !_state.IsInRange) return;

    //Windows ignores the other end while the pen stays in range: a hand turning the pen over takes it away first
    if (state.IsInRange && _state.IsInRange && state.IsEraser != _state.IsEraser)
    {
      auto away = _state;
      away.IsInRange = false;
      away.IsInContact = false;
      Send(away, OptionsOf(_state, away));
      _state = away;
    }

    Send(state, OptionsOf(_state, state));
    _state = state;

    if (_state.IsInRange) _heartbeat.Start();
    else _heartbeat.Stop();
  }

  void PenInjector::Send(const PenState& state, InjectedInputPointerOptions options)
  {
    InjectedInputPointerInfo pointer{};
    pointer.PointerId = 1;
    pointer.PointerOptions = options;
    pointer.PixelLocation = { state.Position.X, state.Position.Y };

    InjectedInputPenInfo pen;
    pen.PointerInfo(pointer);
    pen.Pressure(state.IsInContact ? state.Pressure : 0.);
    pen.TiltX(state.TiltX);
    pen.TiltY(state.TiltY);
    pen.Rotation(state.Rotation);
    pen.PenButtons(ButtonsOf(state));
    pen.PenParameters(InjectedInputPenParameters::Pressure | InjectedInputPenParameters::TiltX | InjectedInputPenParameters::TiltY | InjectedInputPenParameters::Rotation);

    _provider->Injector().InjectPenInput(pen);
    _lastInjection = steady_clock::now();
  }

  void PenInjector::RepeatIfIdle()
  {
    lock_guard lock{ _mutex };
    if (!_state.IsInRange || steady_clock::now() - _lastInjection < _repeatAfter) return;

    Send(_state, OptionsOf(_state, _state));
  }
}
