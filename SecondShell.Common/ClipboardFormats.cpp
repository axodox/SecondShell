#include "pch.h"
#include "ClipboardFormats.h"

using namespace std;

namespace SecondShell::Desktop
{
  namespace
  {
    const pair<uint32_t, const char*> _standardFormats[] = {
      { CF_TEXT, "CF_TEXT" }, { CF_BITMAP, "CF_BITMAP" }, { CF_METAFILEPICT, "CF_METAFILEPICT" }, { CF_SYLK, "CF_SYLK" },
      { CF_DIF, "CF_DIF" }, { CF_TIFF, "CF_TIFF" }, { CF_OEMTEXT, "CF_OEMTEXT" }, { CF_DIB, "CF_DIB" },
      { CF_PALETTE, "CF_PALETTE" }, { CF_PENDATA, "CF_PENDATA" }, { CF_RIFF, "CF_RIFF" }, { CF_WAVE, "CF_WAVE" },
      { CF_UNICODETEXT, "CF_UNICODETEXT" }, { CF_ENHMETAFILE, "CF_ENHMETAFILE" }, { CF_HDROP, "CF_HDROP" },
      { CF_LOCALE, "CF_LOCALE" }, { CF_DIBV5, "CF_DIBV5" }
    };
  }

  optional<uint32_t> ClipboardFormats::FromName(string_view name)
  {
    uint32_t number = 0;
    auto [end, error] = from_chars(name.data(), name.data() + name.size(), number);
    if (error == errc{} && end == name.data() + name.size()) return number;

    for (auto& [format, standardName] : _standardFormats)
    {
      if (_stricmp(standardName, string(name).c_str()) == 0) return format;
    }

    auto format = RegisterClipboardFormatW(winrt::to_hstring(name).c_str());
    if (!format) return nullopt;
    return format;
  }

  string ClipboardFormats::Name(uint32_t format)
  {
    for (auto& [standardFormat, name] : _standardFormats)
    {
      if (standardFormat == format) return name;
    }

    wchar_t name[256];
    auto length = GetClipboardFormatNameW(format, name, int(size(name)));
    return length > 0 ? winrt::to_string(wstring_view(name, length)) : to_string(format);
  }

  bool ClipboardFormats::IsHandle(uint32_t format)
  {
    return format == CF_BITMAP || format == CF_METAFILEPICT || format == CF_PALETTE || format == CF_ENHMETAFILE;
  }
}
