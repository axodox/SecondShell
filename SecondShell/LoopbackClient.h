#pragma once
#include "CredentialProvider.h"
#include "RdpHostWindow.h"
#include "StaThread.h"

namespace SecondShell::Session
{
  //The RDP connection from the host to its own session, through the loopback address (Findings.md 2.17). Signing in
  //this way moves the user's session off this PC's screen, which keeps showing the lock screen, and unlocks it, so the
  //agent's input reaches the child session again. Hidden like the child session's connection: no window, no dialogs.
  class LoopbackClient
  {
    inline static const Axodox::Infrastructure::logger _logger{ "LoopbackClient" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit LoopbackClient(Axodox::Infrastructure::dependency_container* container);
    ~LoopbackClient();

    //Starts connecting with the saved credentials, at the size and scaling of this PC's main display; LoginCompleted
    //or Disconnected follows. False if no credentials are saved; throws if it cannot start.
    bool Connect();
    void Disconnect();

    //Releases a control whose connection ended. Not from an event handler of this class.
    void Release();

    //Raised on the RDP thread.
    Axodox::Infrastructure::event_publisher<LoopbackClient*> LoginCompleted;
    Axodox::Infrastructure::event_publisher<LoopbackClient*, long> Disconnected;  //the disconnect reason

  private:
    std::shared_ptr<CredentialProvider> _credentials;
    StaThread _thread;
    std::unique_ptr<RdpHostWindow> _window;  //used on _thread only

    void OnEvent(std::string_view name, long argument);
  };
}
