#pragma once
#include "AgentControl.h"
#include "DriverLink.h"
#include "ViewTransform.h"

namespace SecondShell::Agent
{
  //The user's mouse, keyboard, touch and pen in the session view, while they have control (Design.md, Visualization):
  //positions mapped to session pixels, sent to the driver at once. The Windows keys stay with the host, whose shell takes their
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

    //A finger or the pen as the view sees it, relative to the view: they go down, move and lift as it says.
    void OnTouch(const winrt::Microsoft::UI::Input::PointerPoint& point, const Rendering::ViewTransform& transform);
    void OnPen(const winrt::Microsoft::UI::Input::PointerPoint& point, const Rendering::ViewTransform& transform);
    //A finger or the pen gone, by a cancel or out of range.
    void OnPointerGone(uint32_t pointerId);

    //When the view loses the focus: the releases would go to the host.
    void ReleaseAll();

  private:
    std::shared_ptr<Session::DriverLink> _driver;
    std::shared_ptr<AgentControl> _agentControl;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;

    std::unordered_set<uint32_t> _touchesDown;
    std::optional<uint32_t> _penInRange;
    bool _isPenInContact = false;

    void Send(Axodox::Infrastructure::value_ptr<Actions::Action>&& input);
    void OnModeChanged(AgentControl* sender, ControlMode mode);
    void ForgetHeldInput();
  };
}
