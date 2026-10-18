#pragma once
#include "DriverProtocol.h"
#include "HostConnection.h"
#include "InputStateTracker.h"
#include "PenActionHandler.h"
#include "TouchActionHandler.h"

namespace SecondShell::Input
{
  //The user's own input in interactive mode (input/forward): the host maps it to session pixels, this plays it, at once
  //and in order. Presses go through InputStateTracker, so a reset releases them as it does the agent's. Touch and pen
  //input are played as the agent's actions are, without durations.
  class UserInputHandler
  {
    inline static const Axodox::Infrastructure::logger _logger{ "UserInputHandler" };

  public:
    //Adds its handler to the host connection, between HostConnection::Open and Hello.
    explicit UserInputHandler(Axodox::Infrastructure::dependency_container* container);

  private:
    std::shared_ptr<Driver::HostConnection> _connection;
    std::shared_ptr<IMouseInjector> _mouse;
    std::shared_ptr<InputStateTracker> _inputState;
    std::shared_ptr<Driver::TouchActionHandler> _touch;
    std::shared_ptr<Driver::PenActionHandler> _pen;
    Rpc::CancellationToken _noCancellation;

    void OnForwardUserInput(const Protocol::ForwardUserInputNotification& notification);
    void Play(const Actions::Action& input);
    void PlayAction(const Actions::Action& input);
  };
}
