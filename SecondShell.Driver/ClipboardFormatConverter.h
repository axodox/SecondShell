#pragma once
#include "ScreenCapturer.h"

namespace SecondShell::Desktop
{
  //Converts between the data of Windows clipboard formats and what the clipboard actions use: UTF-16 text, the
  //"HTML Format" header, CF_HDROP file lists, DIB images, and format names.
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

    //Standard formats by their CF_ names, registered ones by name, any by number; nullopt for an unknown name.
    std::optional<uint32_t> FormatFromName(std::string_view name) const;
    std::string FormatName(uint32_t format) const;

    //Formats whose data is not memory but a GDI handle, which cannot be read or written as bytes.
    bool IsHandleFormat(uint32_t format) const;
  };
}
