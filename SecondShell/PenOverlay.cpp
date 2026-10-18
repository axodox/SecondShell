#include "pch.h"
#include "PenOverlay.h"
#include "PenActions.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  const milliseconds _tapShown{ 150 };
  const milliseconds _trailDuration{ 700 };
  const milliseconds _idleShown{ 3000 };
  const milliseconds _fadeDuration{ 1000 };

  //In device independent pixels
  const float _hoverRadius = 7.f;
  const float _tipRadius = 4.f;
  const float _lineWidth = 2.5f;

  const D2D1_COLOR_F _color{ 0.545f, 0.486f, 1.f, 1.f };  //the agent colour, #8B7CFF

  void PenOverlay::OnEvent(const VisualEvent& event)
  {
    if (event.Result) return;

    auto& action = event.Action;
    switch (action.Type())
    {
    case ActionType::PenMove:
    {
      auto& move = static_cast<const PenMoveAction&>(action);
      MoveTo(move.X, move.Y, event.Time, milliseconds(move.Duration.value_or(0)));
      break;
    }
    case ActionType::PenPress:
    {
      auto& press = static_cast<const PenPressAction&>(action);
      if (press.X && press.Y) MoveTo(*press.X, *press.Y, event.Time);
      if (_path) _contactUntil = time_point::max();
      break;
    }
    case ActionType::PenRelease:
      _contactUntil = event.Time;
      break;
    case ActionType::PenTap:
    {
      auto& tap = static_cast<const PenTapAction&>(action);
      MoveTo(tap.X, tap.Y, event.Time);
      _contactUntil = event.Time + _tapShown;
      break;
    }
    case ActionType::PenReset:
    case ActionType::ControlReset:
      _path.reset();
      _contactUntil = event.Time;
      break;
    default:
      return;
    }

    _lastActivity = max(_lastActivity, event.Time);
  }

  bool PenOverlay::Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now)
  {
    erase_if(_trail, [&](const TrailPoint& point) { return now - point.Time >= _trailDuration; });

    //Where the pen has been while touching the screen; the frames show what it drew
    auto isInContact = IsInContact(now) && _path;
    if (isInContact)
    {
      auto position = _path->At(now);
      auto isStill = _isDrawing && !_trail.empty() && _trail.back().Position.x == position.x && _trail.back().Position.y == position.y;
      if (!isStill) _trail.push_back({ position, now, !_isDrawing });
    }
    _isDrawing = isInContact && !_trail.empty();

    auto visibility = Visibility(now);
    if (_trail.empty() && (!_path || visibility <= 0.f)) return false;

    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(context->CreateSolidColorBrush(_color, brush.put()));
    DrawTrail(context, brush.get(), transform, now);

    if (_path && visibility > 0.f)
    {
      auto center = transform.ToView(_path->At(now));
      brush->SetOpacity(visibility);
      if (isInContact) context->FillEllipse({ center, transform.Ui(_tipRadius), transform.Ui(_tipRadius) }, brush.get());
      context->DrawEllipse({ center, transform.Ui(_hoverRadius), transform.Ui(_hoverRadius) }, brush.get(), transform.Ui(_lineWidth));
    }

    //Until the trail and the idle pen have faded; a pen held still on the screen keeps it as it is
    auto isMoving = _path && _path->IsMoving(now);
    auto isEnding = isInContact && _contactUntil != time_point::max();
    return isMoving || isEnding || !_trail.empty() || (!isInContact && _path && visibility > 0.f);
  }

  void PenOverlay::MoveTo(int32_t x, int32_t y, time_point start, milliseconds duration)
  {
    //A pen out of range comes into range at the target, as the driver brings it
    D2D1_POINT_2F target{ float(x), float(y) };
    _path = Motion{ _path ? _path->At(start) : target, target, start, duration };
    _lastActivity = max(_lastActivity, start + duration);
  }

  bool PenOverlay::IsInContact(time_point now) const
  {
    return now < _contactUntil;
  }

  float PenOverlay::Visibility(time_point now) const
  {
    if (IsInContact(now) || now <= _lastActivity + _idleShown) return 1.f;
    return 1.f - Progress(_lastActivity + _idleShown, _fadeDuration, now);
  }

  void PenOverlay::DrawTrail(ID2D1DeviceContext* context, ID2D1SolidColorBrush* brush, const ViewTransform& transform, time_point now) const
  {
    for (size_t index = 1; index < _trail.size(); index++)
    {
      if (_trail[index].StartsStroke) continue;

      brush->SetOpacity(1.f - Progress(_trail[index].Time, _trailDuration, now));
      context->DrawLine(transform.ToView(_trail[index - 1].Position), transform.ToView(_trail[index].Position), brush, transform.Ui(_lineWidth));
    }
  }
}
