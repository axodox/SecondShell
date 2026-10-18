#include "pch.h"
#include "ClipboardActionHandler.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Actions;
using namespace SecondShell::Capture;
using namespace SecondShell::Desktop;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Driver
{
  //Design.md, Clipboard management; more would not fit in a result the AI could use anyway
  const size_t _rawLimit = 1024 * 1024;
  const size_t _dataLimit = 16 * 1024 * 1024;
  const size_t _imageLimit = 256 * 1024 * 1024;

  namespace
  {
    const string& StringData(const ClipboardContent& content)
    {
      if (!content.Data || content.Data->type() != json_type::string) throw invalid_argument(format("The {} content needs its data as a string.", to_string(content.Format)));
      return content.Data->as<json_string>().value;
    }

    vector<uint8_t> Base64Data(const ClipboardContent& content)
    {
      vector<uint8_t> bytes;
      if (!try_decode_base64(StringData(content), bytes)) throw invalid_argument(format("The {} content's data is not base64.", to_string(content.Format)));
      return bytes;
    }

    vector<uint8_t> ReadFile(const string& path)
    {
      ifstream file(filesystem::path(winrt::to_hstring(path).c_str()), ios::binary);
      if (!file) throw invalid_argument(format("The file {} cannot be read.", path));
      return { istreambuf_iterator<char>(file), istreambuf_iterator<char>() };
    }

    bool IsPng(span<const uint8_t> bytes)
    {
      const uint8_t signature[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
      return bytes.size() >= size(signature) && equal(begin(signature), end(signature), bytes.begin());
    }

    string Join(span<const string> items)
    {
      string result;
      for (auto& item : items) result += (result.empty() ? "" : ", ") + item;
      return result;
    }

    value_ptr<json_value> ToJson(string text)
    {
      return make_value<json_string>(move(text));
    }
  }

  ClipboardActionHandler::ClipboardActionHandler(dependency_container* container) :
    _clipboard(container->resolve<ClipboardManager>()),
    _converter(container->resolve<ClipboardFormatConverter>()),
    _encoder(container->resolve<CaptureEncoder>()),
    _htmlFormat(RegisterClipboardFormatW(L"HTML Format")),
    _pngFormat(RegisterClipboardFormatW(L"PNG"))
  { }

  value_ptr<ActionResult> ClipboardActionHandler::Execute(const Action& action, const CancellationToken& /*cancellation*/)
  {
    try
    {
      switch (action.Type())
      {
      case ActionType::ClipboardRead:
        return Read(static_cast<const ClipboardReadAction&>(action));
      case ActionType::ClipboardWrite:
        return Write(static_cast<const ClipboardWriteAction&>(action));
      default:
        throw logic_error(format("{} is not a clipboard action.", action.Name()));
      }
    }
    catch (const invalid_argument& exception)
    {
      return ActionResult::Failed(ErrorCode::InvalidArgument, exception.what());
    }
    catch (const length_error& exception)
    {
      return ActionResult::Failed(ErrorCode::ActionFailed, exception.what());
    }
  }

  value_ptr<ActionResult> ClipboardActionHandler::Read(const ClipboardReadAction& action)
  {
    auto result = make_value<ClipboardResult>();
    for (auto format : _clipboard->Formats()) result->Formats.push_back(ClipboardFormats::Name(format));

    if (!ReadContent(action, *result))
    {
      if (action.Format) return ActionResult::Failed(ErrorCode::ActionFailed, format("The clipboard has no {}; it has: {}.", to_string(*action.Format), result->Formats.empty() ? "nothing" : Join(result->Formats)));
    }
    return result;
  }

  bool ClipboardActionHandler::ReadContent(const ClipboardReadAction& action, ClipboardResult& result)
  {
    auto format = action.Format.value_or(ClipboardFormat::Text);
    switch (format)
    {
    case ClipboardFormat::Text:
    {
      auto data = _clipboard->Read(CF_UNICODETEXT, _dataLimit);
      if (!data) return false;
      result.Content = ClipboardContent{ .Format = format, .Data = ToJson(_converter->FromUnicodeText(*data)) };
      return true;
    }
    case ClipboardFormat::Html:
    {
      auto data = _clipboard->Read(_htmlFormat, _dataLimit);
      auto fragment = data ? _converter->FromHtmlFormat(*data) : nullopt;
      if (!fragment) return false;
      result.Content = ClipboardContent{ .Format = format, .Data = ToJson(move(*fragment)) };
      return true;
    }
    case ClipboardFormat::Files:
    {
      auto data = _clipboard->Read(CF_HDROP, _dataLimit);
      if (!data) return false;

      auto files = make_value<json_array>();
      for (auto& path : _converter->FromFileDrop(*data)) files->value.push_back(ToJson(path));
      result.Content = ClipboardContent{ .Format = format, .Data = move(files) };
      return true;
    }
    case ClipboardFormat::Image:
      result.Image = ReadImage(action.Resolution);
      return result.Image.has_value();
    case ClipboardFormat::Raw:
    {
      auto rawFormat = RawFormat(action.Name);
      auto data = _clipboard->Read(rawFormat, _rawLimit);
      if (!data) return false;
      result.Content = ClipboardContent{ .Format = format, .Data = ToJson(encode_base64(*data)), .Name = ClipboardFormats::Name(rawFormat) };
      return true;
    }
    default:
      throw logic_error("Unknown clipboard format.");
    }
  }

  optional<CapturedImage> ClipboardActionHandler::ReadImage(const optional<Size>& resolution)
  {
    //PNG keeps transparency; Windows makes a DIB of any bitmap on the clipboard
    if (auto png = _clipboard->Read(_pngFormat, _imageLimit))
    {
      return _encoder->Encode(_encoder->Decode(*png), resolution, ImageFormat::Png);
    }

    for (auto format : { CF_DIBV5, CF_DIB })
    {
      if (auto dib = _clipboard->Read(format, _imageLimit))
      {
        return _encoder->Encode(_encoder->Decode(_converter->DibToBitmapFile(*dib)), resolution, ImageFormat::Png);
      }
    }
    return nullopt;
  }

  value_ptr<ActionResult> ClipboardActionHandler::Write(const ClipboardWriteAction& action)
  {
    //Everything is converted first, so invalid content leaves the clipboard as it was
    vector<ClipboardData> data;
    for (auto& content : action.Content)
    {
      auto converted = ToClipboardData(content);
      data.insert(data.end(), make_move_iterator(converted.begin()), make_move_iterator(converted.end()));
    }

    _clipboard->Write(data);
    return ActionResult::Succeeded();
  }

  vector<ClipboardData> ClipboardActionHandler::ToClipboardData(const ClipboardContent& content) const
  {
    switch (content.Format)
    {
    case ClipboardFormat::Text:
      return { { CF_UNICODETEXT, _converter->ToUnicodeText(StringData(content)) } };
    case ClipboardFormat::Html:
      return { { _htmlFormat, _converter->ToHtmlFormat(StringData(content)) } };
    case ClipboardFormat::Files:
    {
      if (!content.Data || content.Data->type() != json_type::array) throw invalid_argument("The files content needs its data as an array of paths.");

      vector<string> paths;
      for (auto& item : content.Data->as<json_array>().value)
      {
        if (!item || item->type() != json_type::string) throw invalid_argument("The files content needs its data as an array of paths.");
        paths.push_back(item->as<json_string>().value);
      }
      return { { CF_HDROP, _converter->ToFileDrop(paths) } };
    }
    case ClipboardFormat::Image:
    {
      auto bytes = content.Path ? ReadFile(*content.Path) : Base64Data(content);

      //Applications read either the DIB or the PNG
      vector<ClipboardData> data{ { CF_DIBV5, _converter->ToDib(_encoder->Decode(bytes)) } };
      if (IsPng(bytes)) data.push_back({ _pngFormat, move(bytes) });
      return data;
    }
    case ClipboardFormat::Raw:
      return { { RawFormat(content.Name), Base64Data(content) } };
    default:
      throw logic_error("Unknown clipboard format.");
    }
  }

  uint32_t ClipboardActionHandler::RawFormat(const optional<string>& name) const
  {
    if (!name) throw invalid_argument("Raw clipboard content needs the name of its format.");

    auto number = ClipboardFormats::FromName(*name);
    if (!number) throw invalid_argument(format("{} is not a clipboard format.", *name));
    if (ClipboardFormats::IsHandle(*number)) throw invalid_argument(format("{} is a handle, not data; read it as Image or Text instead.", *name));
    return *number;
  }
}
