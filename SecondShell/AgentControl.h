#pragma once

namespace SecondShell::Agent
{
  named_enum(ControlMode,
    Agent,    //the agent may use the session
    Stopped,  //the user stopped the agent
    User      //the user has taken control: their input goes to the session
  );

  //Who controls the session (Design.md, Visualization). The user stops the agent or takes control, which interrupts
  //the running sequence and makes further ones fail until they give control back.
  class AgentControl
  {
    inline static const Axodox::Infrastructure::logger _logger{ "AgentControl" };
    Axodox::Infrastructure::event_owner _events;

  public:
    AgentControl();

    ControlMode Mode() const;
    bool IsAgentAllowed() const;

    void Stop();
    void TakeControl();
    void GiveBackControl();

    Axodox::Infrastructure::event_publisher<AgentControl*, ControlMode> ModeChanged;

  private:
    std::atomic<ControlMode> _mode = ControlMode::Agent;

    void SetMode(ControlMode mode);
  };
}
