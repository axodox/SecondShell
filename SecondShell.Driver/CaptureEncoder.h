#pragma once
#include "ActionValues.h"
#include "ScreenCapturer.h"

namespace SecondShell::Capture
{
  //Scales pixels down to fit a resolution (keeping the aspect ratio, never up) and encodes them to PNG or JPEG; and
  //decodes image files, e.g. of the clipboard.
  class CaptureEncoder
  {
  public:
    CaptureEncoder();

    Actions::CapturedImage Encode(const Pixels& pixels, const std::optional<Actions::Size>& maxResolution, Actions::ImageFormat format) const;

    //Any format Windows reads (PNG, JPEG, BMP, ...), to 32-bit BGRA; throws invalid_argument if it is not an image.
    Pixels Decode(std::span<const uint8_t> file) const;

  private:
    winrt::com_ptr<IWICImagingFactory> _factory;
  };
}
