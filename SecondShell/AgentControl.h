#pragma once

namespace SecondShell::Agent
{
  //Whether the agent may use the session. The user stops it (the Stop button, interactive mode) and gives control back.
  class AgentControl
  {
    Axodox::Infrastructure::event_owner _events;

  public:
    AgentControl();

    bool IsAgentAllowed() const;

    //Interrupts the running sequence; further sequences fail until control is given back.
    void Stop();
    void GiveBackControl();

    Axodox::Infrastructure::event_publisher<AgentControl*> Stopped;

  private:
    std::atomic_bool _isAgentAllowed = true;
  };
}
