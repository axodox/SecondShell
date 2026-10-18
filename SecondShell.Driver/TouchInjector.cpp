#include "pch.h"
#include "TouchInjector.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;
using namespace winrt::Windows::UI::Input::Preview::Injection;

namespace SecondShell::Input
{
  //Repeated sooner than this, Windows would see more input than there is
  const milliseconds _repeatAfter{ 40 };

  //A fingertip, in session pixels from its centre
  const int32_t _contactRadius = 4;
  const double _pressure = 0.8;

  namespace
  {
    InjectedInputPointerOptions OptionsOf(TouchPhase phase)
    {
      using enum InjectedInputPointerOptions;
      switch (phase)
      {
      case TouchPhase::Down:
        return New | PointerDown | InRange | InContact;
      case TouchPhase::Moved:
        return Update | InRange | InContact;
      default:
        return PointerUp;
      }
    }

    InjectedInputTouchInfo TouchInfoOf(const TouchContact& contact)
    {
      InjectedInputPointerInfo pointer{};
      pointer.PointerId = contact.Id;
      pointer.PointerOptions = OptionsOf(contact.Phase);
      pointer.PixelLocation = { contact.Position.X, contact.Position.Y };

      //In absolute screen pixels, in the order left, top, bottom, right (Findings.md 2.14)
      InjectedInputTouchInfo touch;
      touch.PointerInfo(pointer);
      touch.Contact({ contact.Position.X - _contactRadius, contact.Position.Y - _contactRadius, contact.Position.Y + _contactRadius, contact.Position.X + _contactRadius });
      touch.Pressure(_pressure);
      touch.TouchParameters(InjectedInputTouchParameters::Contact | InjectedInputTouchParameters::Pressure);
      return touch;
    }
  }

  TouchInjector::TouchInjector(dependency_container* container) :
    _provider(container->resolve<InputInjectorProvider>()),
    _heartbeat([this] { RepeatIfIdle(); })
  { }

  TouchInjector::~TouchInjector()
  {
    //The heartbeat then finds nothing to repeat
    lock_guard lock{ _mutex };
    _downContacts.clear();
    if (_isInitialized) _provider->Injector().UninitializeTouchInjection();
  }

  void TouchInjector::Inject(span<const TouchContact> frame)
  {
    lock_guard lock{ _mutex };
    if (!_isInitialized)
    {
      _provider->Injector().InitializeTouchInjection(InjectedInputVisualizationMode::Default);
      _isInitialized = true;
    }

    Send(frame);

    _downContacts.clear();
    for (auto& contact : frame)
    {
      if (contact.Phase != TouchPhase::Up) _downContacts.push_back({ contact.Id, contact.Position, TouchPhase::Moved });
    }

    if (_downContacts.empty()) _heartbeat.Stop();
    else _heartbeat.Start();
  }

  void TouchInjector::Send(span<const TouchContact> frame)
  {
    vector<InjectedInputTouchInfo> touches;
    for (auto& contact : frame) touches.push_back(TouchInfoOf(contact));

    _provider->Injector().InjectTouchInput(touches);
    _lastInjection = steady_clock::now();
  }

  void TouchInjector::RepeatIfIdle()
  {
    lock_guard lock{ _mutex };
    if (_downContacts.empty() || steady_clock::now() - _lastInjection < _repeatAfter) return;

    Send(_downContacts);
  }
}
