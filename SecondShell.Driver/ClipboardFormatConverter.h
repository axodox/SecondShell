#pragma once
#include "ScreenCapturer.h"

namespace SecondShell::Desktop
{
  //Converts between the data of Windows clipboard formats and what the clipboard actions use: UTF-16 text, the
  //"HTML Format" header, CF_HDROP file lists and DIB images.
  class ClipboardFormatConverter
  {
  public:
    std::vector<uint8_t> ToUnicodeText(std::string_view text) const;
    std::string FromUnicodeText(std::span<const uint8_t> data) const;

    //Wraps an HTML fragment in the header and document applications expect.
    std::vector<uint8_t> ToHtmlFormat(std::string_view fragment) const;
    std::optional<std::string> FromHtmlFormat(std::span<const uint8_t> data) const;

    std::vector<uint8_t> ToFileDrop(std::span<const std::string> paths) const;
    std::vector<std::string> FromFileDrop(std::span<const uint8_t> data) const;

    //A 32-bit DIB with alpha (BITMAPV5HEADER), as CF_DIBV5 holds it.
    std::vector<uint8_t> ToDib(const Capture::Pixels& pixels) const;

    //A DIB as a .bmp file, which image decoders read.
    std::vector<uint8_t> DibToBitmapFile(std::span<const uint8_t> dib) const;
  };
}
