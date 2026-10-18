#include "pch.h"
#include "UserInputHandler.h"
#include "KeyboardActions.h"
#include "MouseActions.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace std;

namespace SecondShell::Input
{
  UserInputHandler::UserInputHandler(dependency_container* container) :
    _connection(container->resolve<Driver::HostConnection>()),
    _mouse(container->resolve<IMouseInjector>()),
    _inputState(container->resolve<InputStateTracker>())
  {
    _connection->Peer().AddNotificationHandler(this, &UserInputHandler::OnForwardUserInput);
  }

  void UserInputHandler::OnForwardUserInput(const ForwardUserInputNotification& notification)
  {
    //The session guard: nothing is played before the host has accepted this driver
    if (!_connection->IsAccepted()) return;

    for (auto& input : notification.Inputs)
    {
      if (input) Play(*input);
    }
  }

  void UserInputHandler::Play(const Action& input)
  {
    switch (input.Type())
    {
    case ActionType::MouseMove:
    {
      auto& move = static_cast<const MouseMoveAction&>(input);
      _mouse->MoveTo({ move.X, move.Y });
      break;
    }
    case ActionType::MousePress:
      _inputState->Press(static_cast<const MousePressAction&>(input).Button);
      break;
    case ActionType::MouseRelease:
      _inputState->Release(static_cast<const MouseReleaseAction&>(input).Button);
      break;
    case ActionType::MouseWheel:
    {
      auto& wheel = static_cast<const MouseWheelAction&>(input);
      _mouse->Wheel(wheel.Delta, wheel.Axis);
      break;
    }
    case ActionType::KeyboardPress:
      _inputState->Press(static_cast<const KeyboardPressAction&>(input).Key);
      break;
    case ActionType::KeyboardRelease:
      _inputState->Release(static_cast<const KeyboardReleaseAction&>(input).Key);
      break;
    default:
      _logger.log(log_severity::warning, "{} is not user input; ignored.", input.Name());
    }
  }
}
