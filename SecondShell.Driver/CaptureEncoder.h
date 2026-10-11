#pragma once
#include "ActionValues.h"
#include "ScreenCapturer.h"

namespace SecondShell::Capture
{
  //Scales pixels down to fit a resolution (keeping the aspect ratio, never up) and encodes them to PNG or JPEG.
  class CaptureEncoder
  {
  public:
    CaptureEncoder();

    Actions::CapturedImage Encode(const Pixels& pixels, const std::optional<Actions::Size>& maxResolution, Actions::ImageFormat format) const;

  private:
    winrt::com_ptr<IWICImagingFactory> _factory;
  };
}
