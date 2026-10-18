#include "pch.h"
#include "CursorOverlay.h"
#include "MouseActions.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  //A press shorter than this would not show
  const milliseconds _minPressShown{ 150 };
  const milliseconds _doubleClickGap{ 90 };
  const milliseconds _rippleDuration{ 450 };
  const milliseconds _wheelDuration{ 500 };
  const milliseconds _idleShown{ 3000 };
  const milliseconds _fadeDuration{ 1000 };

  //In device independent pixels
  const float _radius = 16.f;
  const float _ringWidth = 2.5f;
  const float _middleRadius = 5.f;

  const D2D1_COLOR_F _color{ 0.545f, 0.486f, 1.f, 1.f };  //the agent colour, #8B7CFF
  const float _pressedOpacity = 0.6f;

  void CursorOverlay::OnEvent(const VisualEvent& event)
  {
    if (event.Result) return;

    auto& action = event.Action;
    switch (action.Type())
    {
    case ActionType::MouseMove:
    {
      auto& move = static_cast<const MouseMoveAction&>(action);
      MoveTo(move.X, move.Y, event.Time, milliseconds(move.Duration.value_or(0)));
      break;
    }
    case ActionType::MouseClick:
    {
      auto& click = static_cast<const MouseClickAction&>(action);
      MoveTo(click.X, click.Y, event.Time);
      Click(click.Button, event.Time, milliseconds(click.Duration.value_or(0)), 1);
      break;
    }
    case ActionType::MouseDoubleClick:
    {
      auto& click = static_cast<const MouseDoubleClickAction&>(action);
      MoveTo(click.X, click.Y, event.Time);
      Click(click.Button, event.Time, {}, 2);
      break;
    }
    case ActionType::MouseWheel:
    {
      auto& wheel = static_cast<const MouseWheelAction&>(action);
      MoveTo(wheel.X, wheel.Y, event.Time);
      Turn(wheel.Axis, wheel.Delta, event.Time);
      break;
    }
    case ActionType::MousePress:
      _presses.push_back({ static_cast<const MousePressAction&>(action).Button, time_point::max() });
      break;
    case ActionType::MouseRelease:
      Release(static_cast<const MouseReleaseAction&>(action).Button);
      break;
    case ActionType::MouseReset:
    case ActionType::ControlReset:
      _presses.clear();
      break;
    default:
      return;
    }

    _lastActivity = max(_lastActivity, event.Time);
  }

  bool CursorOverlay::Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now)
  {
    erase_if(_presses, [&](const Press& press) { return press.Until <= now; });
    erase_if(_ripples, [&](const Ripple& ripple) { return now - ripple.Start >= _rippleDuration; });
    erase_if(_wheelTurns, [&](const WheelTurn& turn) { return now - turn.Start >= _wheelDuration; });

    auto position = Position(now);
    auto visibility = Visibility(now);
    if (!position || visibility <= 0.f) return false;

    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(context->CreateSolidColorBrush(_color, brush.put()));

    DrawRipples(context, brush.get(), transform, now);
    DrawWheelTurns(context, brush.get(), transform, now);

    brush->SetOpacity(visibility);
    DrawPointer(context, brush.get(), transform.ToView(*position), transform.Ui(_radius));

    //Until it has faded out; a held button keeps it as it is
    auto isMoving = _motion && _motion->IsMoving(now);
    auto isEnding = ranges::any_of(_presses, [](const Press& press) { return press.Until != time_point::max(); });
    return isMoving || isEnding || !_ripples.empty() || !_wheelTurns.empty() || _presses.empty();
  }

  optional<D2D1_POINT_2F> CursorOverlay::Position(time_point now) const
  {
    if (!_motion) return nullopt;
    return _motion->At(now);
  }

  void CursorOverlay::MoveTo(optional<int32_t> x, optional<int32_t> y, time_point start, milliseconds duration)
  {
    if (!x || !y) return;

    //From where the pointer is now, or, the first time, where it is unknown, from the target itself
    D2D1_POINT_2F target{ float(*x), float(*y) };
    _motion = Motion{ Position(start).value_or(target), target, start, duration };
    _lastActivity = max(_lastActivity, start + duration);
  }

  void CursorOverlay::Click(MouseButton button, time_point start, milliseconds duration, uint32_t count)
  {
    auto position = Position(start);
    if (!position) return;

    for (uint32_t index = 0; index < count; index++) _ripples.push_back({ *position, start + index * _doubleClickGap });
    _presses.push_back({ button, start + max(duration, _minPressShown) + (count - 1) * _doubleClickGap });
  }

  void CursorOverlay::Turn(WheelAxis axis, float delta, time_point start)
  {
    auto position = Position(start);
    if (!position || delta == 0.f) return;

    _wheelTurns.push_back({ *position, start, axis, delta > 0.f });
  }

  void CursorOverlay::Release(MouseButton button)
  {
    auto held = ranges::find_if(_presses, [&](const Press& press) { return press.Button == button && press.Until == time_point::max(); });
    if (held != _presses.end()) _presses.erase(held);
  }

  float CursorOverlay::Visibility(time_point now) const
  {
    if (!_presses.empty() || now <= _lastActivity + _idleShown) return 1.f;
    return 1.f - Progress(_lastActivity + _idleShown, _fadeDuration, now);
  }

  void CursorOverlay::DrawPointer(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, D2D1_POINT_2F center, float radius) const
  {
    auto ringWidth = radius / _radius * _ringWidth;
    context->DrawEllipse({ center, radius, radius }, brush, ringWidth);

    auto opacity = brush->GetOpacity();
    brush->SetOpacity(opacity * _pressedOpacity);
    for (auto& press : _presses)
    {
      if (press.Button == MouseButton::Middle)
      {
        auto middle = radius / _radius * _middleRadius;
        context->FillEllipse({ center, middle, middle }, brush);
        continue;
      }

      //The pressed button's half of the ring: a half disc, left or right
      winrt::com_ptr<ID2D1Factory> factory;
      context->GetFactory(factory.put());

      winrt::com_ptr<ID2D1PathGeometry> half;
      winrt::check_hresult(factory->CreatePathGeometry(half.put()));

      winrt::com_ptr<ID2D1GeometrySink> sink;
      winrt::check_hresult(half->Open(sink.put()));
      auto side = press.Button == MouseButton::Left ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE;
      sink->BeginFigure({ center.x, center.y - radius }, D2D1_FIGURE_BEGIN_FILLED);
      sink->AddArc({ { center.x, center.y + radius }, { radius, radius }, 0.f, side, D2D1_ARC_SIZE_SMALL });
      sink->EndFigure(D2D1_FIGURE_END_CLOSED);
      winrt::check_hresult(sink->Close());

      context->FillGeometry(half.get(), brush);
    }
    brush->SetOpacity(opacity);
  }

  void CursorOverlay::DrawRipples(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, const ViewTransform& transform, time_point now) const
  {
    for (auto& ripple : _ripples)
    {
      if (now < ripple.Start) continue;

      auto progress = EaseOut(Progress(ripple.Start, _rippleDuration, now));
      auto radius = transform.Ui(_radius * (1.f + 1.5f * progress));
      brush->SetOpacity(1.f - progress);
      context->DrawEllipse({ transform.ToView(ripple.Position), radius, radius }, brush, transform.Ui(_ringWidth));
    }
  }

  void CursorOverlay::DrawWheelTurns(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, const ViewTransform& transform, time_point now) const
  {
    for (auto& turn : _wheelTurns)
    {
      auto progress = Progress(turn.Start, _wheelDuration, now);
      brush->SetOpacity(1.f - progress);

      //A chevron beyond the ring, in the direction of the turn: up or right for forward, moving on as it fades
      auto sign = turn.IsForward ? -1.f : 1.f;
      D2D1_POINT_2F direction = turn.Axis == WheelAxis::Vertical ? D2D1_POINT_2F{ 0.f, sign } : D2D1_POINT_2F{ -sign, 0.f };
      D2D1_POINT_2F across{ direction.y, direction.x };

      auto center = transform.ToView(turn.Position);
      auto distance = transform.Ui(_radius * (1.4f + 0.6f * EaseOut(progress)));
      auto size = transform.Ui(_radius * 0.5f);
      D2D1_POINT_2F tip{ center.x + direction.x * (distance + size), center.y + direction.y * (distance + size) };
      D2D1_POINT_2F base{ center.x + direction.x * distance, center.y + direction.y * distance };

      auto width = transform.Ui(_ringWidth);
      context->DrawLine({ base.x + across.x * size, base.y + across.y * size }, tip, brush, width);
      context->DrawLine({ base.x - across.x * size, base.y - across.y * size }, tip, brush, width);
    }
  }
}
