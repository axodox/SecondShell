#include "pch.h"
#include "CaptureFlashOverlay.h"
#include "CaptureActions.h"

using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  const milliseconds _flashDuration{ 700 };
  const float _borderWidth = 4.f;
  const D2D1_COLOR_F _flashColor{ 1.f, 0.85f, 0.f, 1.f };

  void CaptureFlashOverlay::OnEvent(const VisualEvent& event)
  {
    if (!event.Result || event.Result->Status != ActionStatus::Succeeded) return;

    switch (event.Action.Type())
    {
    case ActionType::CaptureScreen:
    case ActionType::CaptureWindow:
      //The host does not know a window's bounds, so a window capture flashes the screen
      _flashes.push_back({ event.Time });
      break;
    case ActionType::CaptureRegion:
    {
      auto& region = static_cast<const CaptureRegionAction&>(event.Action);
      _flashes.push_back({ event.Time, D2D1_RECT_F{ float(region.X), float(region.Y), float(region.X + int32_t(region.Width)), float(region.Y + int32_t(region.Height)) } });
      break;
    }
    }
  }

  bool CaptureFlashOverlay::Draw(ID2D1DeviceContext* context, const ViewTransform& transform, steady_clock::time_point now)
  {
    erase_if(_flashes, [&](const Flash& flash) { return now - flash.Start >= _flashDuration; });

    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(context->CreateSolidColorBrush(_flashColor, brush.put()));

    auto width = transform.Ui(_borderWidth);
    for (auto& flash : _flashes)
    {
      brush->SetOpacity(1.f - EaseOut(Progress(flash.Start, _flashDuration, now)));

      //Inside the area, so the screen's border shows in full
      auto area = flash.Region ? transform.ToView(*flash.Region) : transform.Session;
      context->DrawRectangle({ area.left + width / 2, area.top + width / 2, area.right - width / 2, area.bottom - width / 2 }, brush.get(), width);
    }

    return !_flashes.empty();
  }
}
