#include "pch.h"
#include "ClipboardMirror.h"
#include "ClipboardFormats.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Desktop
{
  //Larger formats, e.g. a huge image, are not shared
  const size_t _formatLimit = 64 * 1024 * 1024;

  namespace
  {
    //Windows makes these from another format on the clipboard, so sharing that one is enough
    bool IsSynthesizedFrom(uint32_t format, span<const uint32_t> formats)
    {
      auto has = [&](uint32_t other) { return ranges::find(formats, other) != formats.end(); };
      switch (format)
      {
      case CF_TEXT:
      case CF_OEMTEXT:
        return has(CF_UNICODETEXT);
      case CF_DIB:
        return has(CF_DIBV5);
      default:
        return false;
      }
    }

    //Private, display and OLE's own formats mean something only to the application which put them there
    bool IsPrivate(uint32_t format)
    {
      if (format == CF_OWNERDISPLAY || (format >= CF_PRIVATEFIRST && format <= CF_PRIVATELAST) || (format >= CF_GDIOBJFIRST && format <= CF_GDIOBJLAST)) return true;

      auto name = ClipboardFormats::Name(format);
      return name == "DataObject" || name == "Ole Private Data";
    }
  }

  ClipboardMirror::ClipboardMirror(dependency_container* container) :
    Changed(_events),
    _window(container->resolve<MessageWindow>()),
    _clipboard(container->resolve<ClipboardManager>()),
    _messageReceivedSubscription(_window->MessageReceived({ this, &ClipboardMirror::OnMessageReceived }))
  { }

  ClipboardMirror::~ClipboardMirror()
  {
    IsWatching(false);
  }

  void ClipboardMirror::IsWatching(bool value)
  {
    if (_isWatching.exchange(value) == value) return;

    _window->Invoke([&] {
      if (value) winrt::check_bool(AddClipboardFormatListener(_window->Handle()));
      else RemoveClipboardFormatListener(_window->Handle());
      });
  }

  vector<ClipboardContent> ClipboardMirror::Read()
  {
    vector<ClipboardContent> contents;
    for (auto format : FormatsToShare())
    {
      try
      {
        auto data = _clipboard->Read(format, _formatLimit);
        if (!data) continue;

        contents.push_back({ .Format = ClipboardFormat::Raw, .Data = make_value<json_string>(encode_base64(*data)), .Name = ClipboardFormats::Name(format) });
      }
      catch (const length_error& error)
      {
        _logger.log(log_severity::warning, "{} is not shared: {}", ClipboardFormats::Name(format), error.what());
      }
    }
    return contents;
  }

  void ClipboardMirror::Write(const vector<ClipboardContent>& contents)
  {
    vector<ClipboardData> data;
    for (auto& content : contents)
    {
      if (content.Format != ClipboardFormat::Raw || !content.Name || !content.Data || content.Data->type() != json_type::string) continue;

      auto format = ClipboardFormats::FromName(*content.Name);
      ClipboardData item{ .Format = format.value_or(0) };
      if (!format || !try_decode_base64(content.Data->as<json_string>().value, item.Bytes)) continue;
      data.push_back(move(item));
    }

    if (data.empty()) return;

    //Reported as a change on this side too, which must not go back
    _clipboard->Write(data);
    _writtenSequence = GetClipboardSequenceNumber();
  }

  vector<uint32_t> ClipboardMirror::FormatsToShare()
  {
    auto formats = _clipboard->Formats();

    vector<uint32_t> shared;
    ranges::copy_if(formats, back_inserter(shared), [&](uint32_t format) {
      return !ClipboardFormats::IsHandle(format) && !IsPrivate(format) && !IsSynthesizedFrom(format, formats);
      });
    return shared;
  }

  void ClipboardMirror::OnMessageReceived(MessageWindow* /*sender*/, UINT message)
  {
    if (message != WM_CLIPBOARDUPDATE || !_isWatching || GetClipboardSequenceNumber() == _writtenSequence) return;

    try
    {
      auto contents = Read();
      if (!contents.empty()) _events.raise(Changed, this, contents);
    }
    catch (const exception& error)
    {
      _logger.log(log_severity::warning, "The changed clipboard could not be read: {}", error.what());
    }
  }
}
