#pragma once

namespace SecondShell::Rendering
{
  //Where the session is in the view: session pixels to view pixels, and the view's own scale for the overlay's sizes.
  struct ViewTransform
  {
    D2D1_RECT_F Session{};  //the session's area in the view
    float Scale = 1.f;      //view pixels per session pixel
    float UiScale = 1.f;    //view pixels per device independent pixel

    //The largest area of the session's aspect ratio which fits the view, centered.
    static ViewTransform Letterbox(D2D1_SIZE_U view, uint32_t width, uint32_t height, float uiScale);

    D2D1_POINT_2F ToView(D2D1_POINT_2F point) const;
    D2D1_RECT_F ToView(const D2D1_RECT_F& rect) const;

    //A view pixel to the session pixel under it, the nearest one at the session's edge if it is outside.
    D2D1_POINT_2F ToSession(D2D1_POINT_2F point) const;

    //A size given in device independent pixels.
    float Ui(float size) const;
  };
}
