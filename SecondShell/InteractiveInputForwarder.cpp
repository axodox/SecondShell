#include "pch.h"
#include "InteractiveInputForwarder.h"
#include "KeyboardActions.h"
#include "MouseActions.h"
#include "PenActions.h"
#include "TouchActions.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rendering;
using namespace SecondShell::Session;
using namespace std;
using namespace winrt::Microsoft::UI::Input;

namespace SecondShell::Agent
{
  namespace
  {
    POINT ToSession(const PointerPoint& point, const ViewTransform& transform)
    {
      auto position = transform.ToSession({ point.Position().X * transform.UiScale, point.Position().Y * transform.UiScale });
      return { lround(position.x), lround(position.y) };
    }

    //Twist is the pen's rotation; tilts are degrees, as the pen actions take them
    template<typename TAction>
    void ReadPen(TAction& action, const PointerPointProperties& properties)
    {
      action.Pressure = properties.Pressure();
      action.TiltX = int32_t(lround(properties.XTilt()));
      action.TiltY = int32_t(lround(properties.YTilt()));
      action.Rotation = uint32_t(lround(properties.Twist())) % 360;
    }
  }

  InteractiveInputForwarder::InteractiveInputForwarder(dependency_container* container) :
    _driver(container->resolve<DriverLink>()),
    _agentControl(container->resolve<AgentControl>()),
    _modeChangedSubscription(_agentControl->ModeChanged({ this, &InteractiveInputForwarder::OnModeChanged }))
  { }

  bool InteractiveInputForwarder::IsActive() const
  {
    return _agentControl->Mode() == ControlMode::User;
  }

  void InteractiveInputForwarder::OnPointerMoved(D2D1_POINT_2F position, const ViewTransform& transform)
  {
    auto session = transform.ToSession({ position.x * transform.UiScale, position.y * transform.UiScale });

    auto motion = make_value<MouseMoveAction>();
    motion->X = int32_t(lround(session.x));
    motion->Y = int32_t(lround(session.y));
    Send(move(motion));
  }

  void InteractiveInputForwarder::OnButton(MouseButton button, bool isPressed)
  {
    if (isPressed)
    {
      auto press = make_value<MousePressAction>();
      press->Button = button;
      Send(move(press));
    }
    else
    {
      auto release = make_value<MouseReleaseAction>();
      release->Button = button;
      Send(move(release));
    }
  }

  void InteractiveInputForwarder::OnWheel(float notches, WheelAxis axis)
  {
    auto wheel = make_value<MouseWheelAction>();
    wheel->Delta = notches;
    wheel->Axis = axis;
    Send(move(wheel));
  }

  void InteractiveInputForwarder::OnKey(uint16_t virtualKey, bool isPressed)
  {
    //The host's Start menu opens on the release and takes it, so the session would keep the key held
    if (virtualKey == VK_LWIN || virtualKey == VK_RWIN) return;

    if (isPressed)
    {
      auto press = make_value<KeyboardPressAction>();
      press->Key = virtualKey;
      Send(move(press));
    }
    else
    {
      auto release = make_value<KeyboardReleaseAction>();
      release->Key = virtualKey;
      Send(move(release));
    }
  }

  void InteractiveInputForwarder::OnTouch(const PointerPoint& point, const ViewTransform& transform)
  {
    auto id = point.PointerId();
    auto position = ToSession(point, transform);
    auto wasDown = _touchesDown.contains(id);

    if (point.IsInContact() && !wasDown)
    {
      auto press = make_value<TouchPressAction>();
      press->Id = id;
      press->X = position.x;
      press->Y = position.y;
      Send(move(press));
      _touchesDown.insert(id);
    }
    else if (point.IsInContact())
    {
      auto motion = make_value<TouchMoveAction>();
      motion->Id = id;
      motion->X = position.x;
      motion->Y = position.y;
      Send(move(motion));
    }
    else if (wasDown)
    {
      OnPointerGone(id);
    }
  }

  void InteractiveInputForwarder::OnPen(const PointerPoint& point, const ViewTransform& transform)
  {
    auto position = ToSession(point, transform);
    auto properties = point.Properties();
    _penInRange = point.PointerId();

    if (point.IsInContact() && !_isPenInContact)
    {
      auto press = make_value<PenPressAction>();
      press->X = position.x;
      press->Y = position.y;
      ReadPen(*press, properties);
      press->Barrel = properties.IsBarrelButtonPressed();
      press->Eraser = properties.IsEraser() || properties.IsInverted();
      Send(move(press));
      _isPenInContact = true;
      return;
    }

    if (!point.IsInContact() && _isPenInContact)
    {
      Send(make_value<PenReleaseAction>());
      _isPenInContact = false;
    }

    auto motion = make_value<PenMoveAction>();
    motion->X = position.x;
    motion->Y = position.y;
    if (_isPenInContact) ReadPen(*motion, properties);
    Send(move(motion));
  }

  void InteractiveInputForwarder::OnPointerGone(uint32_t pointerId)
  {
    if (_touchesDown.erase(pointerId))
    {
      auto release = make_value<TouchReleaseAction>();
      release->Id = pointerId;
      Send(move(release));
    }
    else if (_penInRange == pointerId)
    {
      Send(make_value<PenResetAction>());
      _penInRange.reset();
      _isPenInContact = false;
    }
  }

  void InteractiveInputForwarder::ReleaseAll()
  {
    if (IsActive() && _driver->IsConnected()) _driver->ResetInput();
    ForgetHeldInput();
  }

  void InteractiveInputForwarder::Send(value_ptr<Action>&& input)
  {
    if (!IsActive()) return;

    ForwardUserInputNotification notification;
    notification.Inputs.push_back(move(input));
    _driver->ForwardUserInput(notification);
  }

  void InteractiveInputForwarder::OnModeChanged(AgentControl* /*sender*/, ControlMode mode)
  {
    //Nothing the user held stays held for the agent; switching to the user, SequenceRunner releases the agent's
    ForgetHeldInput();
    if (mode == ControlMode::User || !_driver->IsConnected()) return;
    _driver->ResetInput();
  }

  void InteractiveInputForwarder::ForgetHeldInput()
  {
    _touchesDown.clear();
    _penInRange.reset();
    _isPenInContact = false;
  }
}
