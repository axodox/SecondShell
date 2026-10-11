#pragma once
#include "CredentialProvider.h"
#include "HostProtocol.h"
#include "RdpHostWindow.h"
#include "StaThread.h"

namespace SecondShell::Session
{
  //The RDP connection to the child session, on a thread of its own (Findings.md 2.11). Knows nothing about drivers or
  //agents.
  class RdpClient
  {
    inline static const Axodox::Infrastructure::logger _logger{ "RdpClient" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit RdpClient(Axodox::Infrastructure::dependency_container* container);
    ~RdpClient();

    //Starts connecting; LoginCompleted or Disconnected follows. Throws if it cannot start.
    void Connect(const Protocol::DisplaySettings& display);
    void Disconnect();

    //Raised on the RDP thread.
    Axodox::Infrastructure::event_publisher<RdpClient*> LoginCompleted;
    Axodox::Infrastructure::event_publisher<RdpClient*, long> Disconnected;  //the disconnect reason

  private:
    std::shared_ptr<CredentialProvider> _credentials;
    StaThread _thread;
    std::unique_ptr<RdpHostWindow> _window;  //used on _thread only

    void OnEvent(std::string_view name, long argument);
  };
}
