#include "pch.h"
#include "ViewTransform.h"

using namespace std;

namespace SecondShell::Rendering
{
  ViewTransform ViewTransform::Letterbox(D2D1_SIZE_U view, uint32_t width, uint32_t height, float uiScale)
  {
    auto scale = min(float(view.width) / width, float(view.height) / height);
    auto left = (view.width - width * scale) / 2;
    auto top = (view.height - height * scale) / 2;
    return { { left, top, left + width * scale, top + height * scale }, scale, uiScale };
  }

  D2D1_POINT_2F ViewTransform::ToView(D2D1_POINT_2F point) const
  {
    return { Session.left + point.x * Scale, Session.top + point.y * Scale };
  }

  D2D1_RECT_F ViewTransform::ToView(const D2D1_RECT_F& rect) const
  {
    auto topLeft = ToView(D2D1_POINT_2F{ rect.left, rect.top });
    auto bottomRight = ToView(D2D1_POINT_2F{ rect.right, rect.bottom });
    return { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
  }

  D2D1_POINT_2F ViewTransform::ToSession(D2D1_POINT_2F point) const
  {
    auto width = (Session.right - Session.left) / Scale;
    auto height = (Session.bottom - Session.top) / Scale;
    return {
      clamp((point.x - Session.left) / Scale, 0.f, max(width - 1.f, 0.f)),
      clamp((point.y - Session.top) / Scale, 0.f, max(height - 1.f, 0.f))
    };
  }

  float ViewTransform::Ui(float size) const
  {
    return size * UiScale;
  }
}
