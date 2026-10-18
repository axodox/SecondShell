#pragma once
#include "ActionValues.h"
#include "IKeyboardInjector.h"
#include "IMouseInjector.h"
#include "IPenInjector.h"
#include "ITouchInjector.h"

namespace SecondShell::Input
{
  //Presses and releases buttons, keys, touch points and the pen, and remembers what is held, so that it can be
  //reported and released (Control.Reset, a failed sequence, the Stop button).
  class InputStateTracker
  {
  public:
    explicit InputStateTracker(Axodox::Infrastructure::dependency_container* container);

    void Press(Actions::MouseButton button);
    void Release(Actions::MouseButton button);
    void Press(uint16_t virtualKey);
    void Release(uint16_t virtualKey);

    //Where the touch point is, if it is down.
    std::optional<Point> TouchPosition(uint32_t id) const;
    void Touch(uint32_t id, Point position);
    //False if the touch point is not down, e.g. lifted by a reset meanwhile.
    bool MoveTouch(uint32_t id, Point position);
    void Lift(uint32_t id);

    PenState Pen() const;
    void SetPen(const PenState& state);

    //Each returns what it released, in the order it was pressed.
    Actions::InputState ReleaseMouseButtons();
    Actions::InputState ReleaseKeys();
    Actions::InputState ReleaseTouchPoints();
    Actions::InputState ReleasePen();
    Actions::InputState ReleaseAll();

  private:
    std::shared_ptr<IMouseInjector> _mouse;
    std::shared_ptr<IKeyboardInjector> _keyboard;
    std::shared_ptr<ITouchInjector> _touch;
    std::shared_ptr<IPenInjector> _pen;

    mutable std::mutex _mutex;
    std::vector<Actions::MouseButton> _mouseButtons;
    std::vector<uint16_t> _keys;
    std::vector<std::pair<uint32_t, Point>> _touchPoints;
    PenState _penState;

    std::optional<Point> FindTouch(uint32_t id) const;
    //All touch points, the one given in the phase given
    std::vector<TouchContact> TouchFrame(uint32_t id, TouchPhase phase) const;
  };
}
