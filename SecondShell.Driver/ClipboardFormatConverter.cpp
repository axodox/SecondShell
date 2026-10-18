#include "pch.h"
#include "ClipboardFormatConverter.h"

using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Desktop
{
  namespace
  {
    template<typename T>
    T ReadStruct(span<const uint8_t> data, size_t offset = 0)
    {
      if (offset + sizeof(T) > data.size()) throw invalid_argument("The clipboard data is shorter than its format requires.");

      T value;
      memcpy(&value, data.data() + offset, sizeof(T));
      return value;
    }

    template<typename T>
    void WriteStruct(vector<uint8_t>& data, const T& value)
    {
      auto bytes = reinterpret_cast<const uint8_t*>(&value);
      data.insert(data.end(), bytes, bytes + sizeof(T));
    }

    //The UTF-16 text up to its terminating null, or the end of the data
    wstring_view Utf16(span<const uint8_t> data)
    {
      wstring_view text{ reinterpret_cast<const wchar_t*>(data.data()), data.size() / sizeof(wchar_t) };
      return text.substr(0, text.find(L'\0'));
    }

    optional<size_t> HeaderValue(string_view header, string_view name)
    {
      auto position = header.find(name);
      if (position == string_view::npos) return nullopt;

      size_t value = 0;
      auto digits = header.substr(position + name.size());
      auto [end, error] = from_chars(digits.data(), digits.data() + digits.size(), value);
      if (error != errc{}) return nullopt;
      return value;
    }

    //Offsets are byte offsets in the data; written with a fixed width, so the header's length does not depend on them
    string HtmlHeader(size_t startHtml, size_t endHtml, size_t startFragment, size_t endFragment)
    {
      return format("Version:0.9\r\nStartHTML:{:010}\r\nEndHTML:{:010}\r\nStartFragment:{:010}\r\nEndFragment:{:010}\r\n", startHtml, endHtml, startFragment, endFragment);
    }
  }

  vector<uint8_t> ClipboardFormatConverter::ToUnicodeText(string_view text) const
  {
    auto utf16 = winrt::to_hstring(text);
    vector<uint8_t> data(reinterpret_cast<const uint8_t*>(utf16.c_str()), reinterpret_cast<const uint8_t*>(utf16.c_str() + utf16.size() + 1));
    return data;
  }

  string ClipboardFormatConverter::FromUnicodeText(span<const uint8_t> data) const
  {
    return winrt::to_string(Utf16(data));
  }

  vector<uint8_t> ClipboardFormatConverter::ToHtmlFormat(string_view fragment) const
  {
    const string_view prefix = "<html><body>\r\n<!--StartFragment-->";
    const string_view suffix = "<!--EndFragment-->\r\n</body></html>";

    auto startHtml = HtmlHeader(0, 0, 0, 0).size();
    auto startFragment = startHtml + prefix.size();
    auto endFragment = startFragment + fragment.size();
    auto endHtml = endFragment + suffix.size();

    auto text = HtmlHeader(startHtml, endHtml, startFragment, endFragment);
    text.append(prefix).append(fragment).append(suffix);

    vector<uint8_t> data(text.begin(), text.end());
    data.push_back(0);
    return data;
  }

  optional<string> ClipboardFormatConverter::FromHtmlFormat(span<const uint8_t> data) const
  {
    string_view text{ reinterpret_cast<const char*>(data.data()), data.size() };
    text = text.substr(0, text.find('\0'));

    auto start = HeaderValue(text, "StartFragment:");
    auto end = HeaderValue(text, "EndFragment:");
    if (!start || !end || *start > *end || *end > text.size()) return nullopt;
    return string(text.substr(*start, *end - *start));
  }

  vector<uint8_t> ClipboardFormatConverter::ToFileDrop(span<const string> paths) const
  {
    //The header, then each path with its null, then an empty one
    vector<uint8_t> data;
    WriteStruct(data, DROPFILES{ .pFiles = sizeof(DROPFILES), .fWide = TRUE });
    for (auto& path : paths)
    {
      auto utf16 = winrt::to_hstring(path);
      data.insert(data.end(), reinterpret_cast<const uint8_t*>(utf16.c_str()), reinterpret_cast<const uint8_t*>(utf16.c_str() + utf16.size() + 1));
    }
    WriteStruct(data, L'\0');
    return data;
  }

  vector<string> ClipboardFormatConverter::FromFileDrop(span<const uint8_t> data) const
  {
    auto header = ReadStruct<DROPFILES>(data);
    if (header.pFiles > data.size()) throw invalid_argument("The file list is shorter than its header says.");
    auto files = data.subspan(header.pFiles);

    vector<string> paths;
    if (header.fWide)
    {
      wstring_view text{ reinterpret_cast<const wchar_t*>(files.data()), files.size() / sizeof(wchar_t) };
      for (size_t start = 0, end; start < text.size() && text[start] != L'\0'; start = end + 1)
      {
        end = min(text.find(L'\0', start), text.size());
        paths.push_back(winrt::to_string(text.substr(start, end - start)));
      }
    }
    else
    {
      string_view text{ reinterpret_cast<const char*>(files.data()), files.size() };
      for (size_t start = 0, end; start < text.size() && text[start] != '\0'; start = end + 1)
      {
        end = min(text.find('\0', start), text.size());
        auto path = text.substr(start, end - start);
        wstring utf16(size_t(MultiByteToWideChar(CP_ACP, 0, path.data(), int(path.size()), nullptr, 0)), L'\0');
        MultiByteToWideChar(CP_ACP, 0, path.data(), int(path.size()), utf16.data(), int(utf16.size()));
        paths.push_back(winrt::to_string(utf16));
      }
    }
    return paths;
  }

  vector<uint8_t> ClipboardFormatConverter::ToDib(const Pixels& pixels) const
  {
    auto rowSize = size_t(pixels.Width) * 4;
    BITMAPV5HEADER header{
      .bV5Size = sizeof(BITMAPV5HEADER),
      .bV5Width = LONG(pixels.Width),
      .bV5Height = LONG(pixels.Height),
      .bV5Planes = 1,
      .bV5BitCount = 32,
      .bV5Compression = BI_BITFIELDS,
      .bV5SizeImage = DWORD(rowSize * pixels.Height),
      .bV5RedMask = 0x00FF0000,
      .bV5GreenMask = 0x0000FF00,
      .bV5BlueMask = 0x000000FF,
      .bV5AlphaMask = 0xFF000000,
      .bV5CSType = LCS_sRGB,
      .bV5Intent = LCS_GM_IMAGES
    };

    //A positive height: the rows bottom-up, which every application reads
    vector<uint8_t> data;
    WriteStruct(data, header);
    for (auto row = pixels.Height; row-- > 0;)
    {
      auto start = pixels.Bgra.begin() + ptrdiff_t(row * rowSize);
      data.insert(data.end(), start, start + ptrdiff_t(rowSize));
    }
    return data;
  }

  vector<uint8_t> ClipboardFormatConverter::DibToBitmapFile(span<const uint8_t> dib) const
  {
    auto header = ReadStruct<BITMAPINFOHEADER>(dib);

    //The pixels follow the header, the bit masks of a plain BITMAPINFOHEADER, and the color table
    size_t bitsOffset = header.biSize;
    if (header.biSize == sizeof(BITMAPINFOHEADER) && header.biCompression == BI_BITFIELDS) bitsOffset += 3 * sizeof(DWORD);
    auto colors = header.biClrUsed ? header.biClrUsed : header.biBitCount <= 8 ? 1u << header.biBitCount : 0u;
    bitsOffset += colors * sizeof(RGBQUAD);

    vector<uint8_t> file;
    WriteStruct(file, BITMAPFILEHEADER{
      .bfType = 0x4D42,  //"BM"
      .bfSize = DWORD(sizeof(BITMAPFILEHEADER) + dib.size()),
      .bfOffBits = DWORD(sizeof(BITMAPFILEHEADER) + bitsOffset)
      });
    file.insert(file.end(), dib.begin(), dib.end());
    return file;
  }
}
