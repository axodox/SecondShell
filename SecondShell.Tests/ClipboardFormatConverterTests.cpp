#include "pch.h"
#include "CaptureEncoder.h"
#include "ClipboardFormatConverter.h"
#include "ClipboardFormats.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Capture;
using namespace SecondShell::Desktop;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ClipboardFormatConverterTests)
  {
    ClipboardFormatConverter _converter;

  public:
    TEST_METHOD(TextIsNullTerminatedUtf16)
    {
      auto data = _converter.ToUnicodeText("Árvíztűrő 🙂");
      Assert::AreEqual(size_t(0), size_t(data.back() | data[data.size() - 2]), L"the text should end with a null");
      Assert::AreEqual(string("Árvíztűrő 🙂"), _converter.FromUnicodeText(data));
    }

    TEST_METHOD(HtmlHasAHeaderWithByteOffsets)
    {
      auto data = _converter.ToHtmlFormat("<b>ő</b>");
      string text(data.begin(), data.end() - 1);
      Assert::IsTrue(text.starts_with("Version:0.9\r\n"));

      //The offsets point into the data
      auto offset = [&](string_view name) { return stoul(text.substr(text.find(name) + name.size(), 10)); };
      Assert::AreEqual(string("<html>"), text.substr(offset("StartHTML:"), 6));
      Assert::AreEqual(text.size(), size_t(offset("EndHTML:")));
      Assert::AreEqual(string("<b>ő</b>"), text.substr(offset("StartFragment:"), offset("EndFragment:") - offset("StartFragment:")), L"the offsets count bytes, not characters");

      Assert::AreEqual(string("<b>ő</b>"), *_converter.FromHtmlFormat(data));
    }

    TEST_METHOD(HtmlWithoutAHeaderIsNotRead)
    {
      string text = "<b>no header</b>";
      Assert::IsFalse(_converter.FromHtmlFormat({ reinterpret_cast<const uint8_t*>(text.data()), text.size() }).has_value());
    }

    TEST_METHOD(FileListsRoundTrip)
    {
      vector<string> paths{ "C:\\Temp\\a.txt", "D:\\Mappa\\ű.png" };
      Assert::IsTrue(paths == _converter.FromFileDrop(_converter.ToFileDrop(paths)));
    }

    TEST_METHOD(DibsCanBeDecoded)
    {
      //2x2: blue, green / red, white, top-down
      Pixels pixels{ 2, 2, { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255 } };
      auto file = _converter.DibToBitmapFile(_converter.ToDib(pixels));

      auto decoded = CaptureEncoder().Decode(file);
      Assert::AreEqual(2u, decoded.Width);
      Assert::AreEqual(2u, decoded.Height);
      Assert::IsTrue(pixels.Bgra == decoded.Bgra, L"the rows should come back in order");
    }

    TEST_METHOD(FormatsAreFoundByName)
    {
      Assert::AreEqual(uint32_t(CF_UNICODETEXT), *ClipboardFormats::FromName("CF_UNICODETEXT"));
      Assert::AreEqual(uint32_t(CF_UNICODETEXT), *ClipboardFormats::FromName("cf_unicodetext"));
      Assert::AreEqual(uint32_t(13), *ClipboardFormats::FromName("13"));
      Assert::AreEqual(string("CF_HDROP"), ClipboardFormats::Name(CF_HDROP));

      auto html = ClipboardFormats::FromName("HTML Format");
      Assert::IsTrue(html.has_value());
      Assert::AreEqual(string("HTML Format"), ClipboardFormats::Name(*html));
    }

    TEST_METHOD(HandleFormatsAreKnown)
    {
      Assert::IsTrue(ClipboardFormats::IsHandle(CF_BITMAP));
      Assert::IsFalse(ClipboardFormats::IsHandle(CF_DIBV5));
    }
  };
}
