#include "pch.h"
#include "TouchOverlay.h"
#include "TouchActions.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  //A tap would not show otherwise
  const milliseconds _tapShown{ 150 };
  const milliseconds _liftDuration{ 450 };

  //In device independent pixels
  const float _radius = 14.f;
  const float _ringWidth = 2.5f;
  const float _fillOpacity = 0.35f;

  const D2D1_COLOR_F _color{ 0.545f, 0.486f, 1.f, 1.f };  //the agent colour, #8B7CFF

  void TouchOverlay::OnEvent(const VisualEvent& event)
  {
    if (event.Result) return;

    auto& action = event.Action;
    switch (action.Type())
    {
    case ActionType::TouchPress:
    {
      auto& press = static_cast<const TouchPressAction&>(action);
      Press(press.Id, press.X, press.Y, event.Time);
      break;
    }
    case ActionType::TouchTap:
    {
      auto& tap = static_cast<const TouchTapAction&>(action);
      Press(tap.Id, tap.X, tap.Y, event.Time, event.Time + _tapShown);
      _lifts.push_back({ { float(tap.X), float(tap.Y) }, event.Time + _tapShown });
      break;
    }
    case ActionType::TouchMove:
    {
      auto& move = static_cast<const TouchMoveAction&>(action);
      Move(move.Id, move.X, move.Y, event.Time, milliseconds(move.Duration.value_or(0)));
      break;
    }
    case ActionType::TouchRelease:
      Release(static_cast<const TouchReleaseAction&>(action).Id, event.Time);
      break;
    case ActionType::TouchReset:
    case ActionType::ControlReset:
      ReleaseAll(event.Time);
      break;
    }
  }

  bool TouchOverlay::Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now)
  {
    erase_if(_fingers, [&](const Finger& finger) { return finger.Until <= now; });
    erase_if(_lifts, [&](const Lift& lift) { return now - lift.Start >= _liftDuration; });
    if (_fingers.empty() && _lifts.empty()) return false;

    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(context->CreateSolidColorBrush(_color, brush.put()));

    auto radius = transform.Ui(_radius);
    for (auto& finger : _fingers)
    {
      D2D1_ELLIPSE disc{ transform.ToView(finger.Path.At(now)), radius, radius };
      brush->SetOpacity(_fillOpacity);
      context->FillEllipse(disc, brush.get());
      brush->SetOpacity(1.f);
      context->DrawEllipse(disc, brush.get(), transform.Ui(_ringWidth));
    }

    for (auto& lift : _lifts)
    {
      if (now < lift.Start) continue;

      auto progress = EaseOut(Progress(lift.Start, _liftDuration, now));
      auto liftRadius = transform.Ui(_radius * (1.f + 1.5f * progress));
      brush->SetOpacity(1.f - progress);
      context->DrawEllipse({ transform.ToView(lift.Position), liftRadius, liftRadius }, brush.get(), transform.Ui(_ringWidth));
    }

    //Fingers held still change nothing until the next event
    auto isAnimating = !_lifts.empty();
    for (auto& finger : _fingers) isAnimating |= finger.Path.IsMoving(now) || finger.Until != time_point::max();
    return isAnimating;
  }

  void TouchOverlay::Press(uint32_t id, int32_t x, int32_t y, time_point start, time_point until)
  {
    //The driver refuses a finger already down; so the overlay keeps the first
    if (ranges::any_of(_fingers, [&](const Finger& finger) { return finger.Id == id && finger.Until == time_point::max(); })) return;

    D2D1_POINT_2F position{ float(x), float(y) };
    _fingers.push_back({ id, { position, position, start }, until });
  }

  void TouchOverlay::Move(uint32_t id, int32_t x, int32_t y, time_point start, milliseconds duration)
  {
    auto finger = ranges::find_if(_fingers, [&](const Finger& finger) { return finger.Id == id && finger.Until == time_point::max(); });
    if (finger == _fingers.end()) return;

    finger->Path = { finger->Path.At(start), { float(x), float(y) }, start, duration };
  }

  void TouchOverlay::Release(uint32_t id, time_point when)
  {
    auto finger = ranges::find_if(_fingers, [&](const Finger& finger) { return finger.Id == id && finger.Until == time_point::max(); });
    if (finger == _fingers.end()) return;

    _lifts.push_back({ finger->Path.At(when), when });
    _fingers.erase(finger);
  }

  void TouchOverlay::ReleaseAll(time_point when)
  {
    for (auto& finger : _fingers)
    {
      if (finger.Until == time_point::max()) _lifts.push_back({ finger.Path.At(when), when });
    }
    erase_if(_fingers, [](const Finger& finger) { return finger.Until == time_point::max(); });
  }
}
