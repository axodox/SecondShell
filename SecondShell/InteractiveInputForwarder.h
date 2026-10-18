#pragma once
#include "AgentControl.h"
#include "DriverLink.h"
#include "ViewTransform.h"

namespace SecondShell::Agent
{
  //The user's mouse and keyboard in the session view, while they have control (Design.md, Visualization): positions
  //mapped to session pixels, sent to the driver at once. The Windows keys stay with the host, whose shell takes their
  //release. Giving control back, or the view losing the focus, releases whatever they still hold.
  class InteractiveInputForwarder
  {
  public:
    explicit InteractiveInputForwarder(Axodox::Infrastructure::dependency_container* container);

    bool IsActive() const;

    //A position in the view's device independent pixels.
    void OnPointerMoved(D2D1_POINT_2F position, const Rendering::ViewTransform& transform);
    void OnButton(Actions::MouseButton button, bool isPressed);
    void OnWheel(float notches, Actions::WheelAxis axis);
    void OnKey(uint16_t virtualKey, bool isPressed);

    //When the view loses the focus: the releases would go to the host.
    void ReleaseAll();

  private:
    std::shared_ptr<Session::DriverLink> _driver;
    std::shared_ptr<AgentControl> _agentControl;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;

    void Send(Axodox::Infrastructure::value_ptr<Actions::Action>&& input);
    void OnModeChanged(AgentControl* sender, ControlMode mode);
  };
}
