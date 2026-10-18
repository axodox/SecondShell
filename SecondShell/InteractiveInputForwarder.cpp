#include "pch.h"
#include "InteractiveInputForwarder.h"
#include "KeyboardActions.h"
#include "MouseActions.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rendering;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Agent
{
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

  void InteractiveInputForwarder::ReleaseAll()
  {
    if (IsActive() && _driver->IsConnected()) _driver->ResetInput();
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
    if (mode == ControlMode::User || !_driver->IsConnected()) return;
    _driver->ResetInput();
  }
}
