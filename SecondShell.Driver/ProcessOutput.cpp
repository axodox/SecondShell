#include "pch.h"
#include "ProcessOutput.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Desktop
{
  //A tail which has grown this much beyond its size is cut back, a line at a time where it can
  const size_t _tailSlack = 4;
  const size_t _lineSearch = 4096;

  namespace
  {
    bool IsContinuation(char character)
    {
      return (uint8_t(character) & 0xC0) == 0x80;
    }

    bool IsUtf8(string_view text)
    {
      return text.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0) > 0;
    }

    //The length without a character the text ends in the middle of
    size_t CompleteLength(string_view text)
    {
      for (size_t back = 1; back <= 4 && back <= text.size(); back++)
      {
        auto lead = uint8_t(text[text.size() - back]);
        if ((lead & 0xC0) == 0x80) continue;

        auto length = lead < 0x80 ? 1u : (lead & 0xE0) == 0xC0 ? 2u : (lead & 0xF0) == 0xE0 ? 3u : (lead & 0xF8) == 0xF0 ? 4u : 1u;
        return back < length ? text.size() - back : text.size();
      }
      return text.size();
    }

    //A position moved back to the start of the character it is in
    size_t BackToBoundary(string_view text, size_t position)
    {
      while (position > 0 && position < text.size() && IsContinuation(text[position])) position--;
      return position;
    }

    string FromOem(string_view bytes)
    {
      if (bytes.empty()) return {};

      wstring utf16(size_t(MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), int(bytes.size()), nullptr, 0)), L'\0');
      MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), int(bytes.size()), utf16.data(), int(utf16.size()));
      return winrt::to_string(utf16);
    }

    char Lower(char character)
    {
      return character >= 'A' && character <= 'Z' ? char(character - 'A' + 'a') : character;
    }
  }

#pragma region OutputDecoder
  string OutputDecoder::Decode(string_view bytes)
  {
    _pending.append(bytes);

    //A byte order mark, which PowerShell may write, is not part of the text
    if (_isStart)
    {
      string_view mark = "\xEF\xBB\xBF";
      if (_pending.size() < mark.size() && mark.starts_with(_pending)) return {};
      if (_pending.starts_with(mark)) _pending.erase(0, mark.size());
      _isStart = false;
    }

    if (!_isOem)
    {
      auto length = CompleteLength(_pending);
      if (IsUtf8(string_view(_pending).substr(0, length)))
      {
        string text = _pending.substr(0, length);
        _pending.erase(0, length);
        return text;
      }
      _isOem = true;
    }

    return FromOem(exchange(_pending, {}));
  }

  string OutputDecoder::Finish()
  {
    auto rest = exchange(_pending, {});
    return !_isOem && IsUtf8(rest) ? rest : FromOem(rest);
  }
#pragma endregion

#pragma region ProcessOutput
  ProcessOutput::ProcessOutput(filesystem::path file, uint64_t partSize, size_t tailSize) :
    _file(move(file)),
    _partSize(max<uint64_t>(partSize, 1)),
    _tailSize(max<size_t>(tailSize, 1))
  { }

  void ProcessOutput::Append(string_view text)
  {
    if (text.empty()) return;

    {
      lock_guard lock{ _mutex };
      Write(text);
      _tail.append(text);
      _size += text.size();

      if (_tail.size() > _tailSize + _tailSize / _tailSlack)
      {
        auto cut = _tail.size() - _tailSize;
        auto newline = _tail.find('\n', cut);
        cut = newline != string::npos && newline - cut < _lineSearch ? newline + 1 : cut;
        while (cut < _tail.size() && IsContinuation(_tail[cut])) cut++;

        _tail.erase(0, cut);
        _tailStart += cut;
      }
    }
    _changed.notify_all();
  }

  void ProcessOutput::End(optional<int32_t> exitCode)
  {
    {
      lock_guard lock{ _mutex };
      _isRunning = false;
      _exitCode = exitCode;
    }
    _changed.notify_all();
  }

  const filesystem::path& ProcessOutput::File() const
  {
    return _file;
  }

  uint64_t ProcessOutput::Size() const
  {
    lock_guard lock{ _mutex };
    return _size;
  }

  uint64_t ProcessOutput::TailStart() const
  {
    lock_guard lock{ _mutex };
    return _tailStart;
  }

  bool ProcessOutput::IsRunning() const
  {
    lock_guard lock{ _mutex };
    return _isRunning;
  }

  optional<int32_t> ProcessOutput::ExitCode() const
  {
    lock_guard lock{ _mutex };
    return _exitCode;
  }

  OutputChunk ProcessOutput::Read(uint64_t from, size_t maxBytes) const
  {
    lock_guard lock{ _mutex };
    from = min(from, _size);

    string text;
    if (from < _tailStart) text = ReadFiles(from, maxBytes);

    //From memory, also when the files cannot be read any more
    if (from >= _tailStart || text.empty())
    {
      from = max(from, _tailStart);
      text = _tail.substr(size_t(from - _tailStart), maxBytes);
    }

    //Whole characters only
    size_t skipped = 0;
    while (skipped < text.size() && skipped < 3 && IsContinuation(text[skipped])) skipped++;
    text.erase(0, skipped);
    from += skipped;
    text.resize(CompleteLength(text));

    auto next = from + text.size();
    return { from, move(text), next, next < _size };
  }

  OutputChunk ProcessOutput::Tail(uint32_t lines, size_t maxBytes) const
  {
    lock_guard lock{ _mutex };

    //A final line break does not count as a line of its own
    auto end = _tail.size();
    if (end > 0 && _tail[end - 1] == '\n') end--;

    auto start = end;
    uint32_t count = 0;
    while (start > 0)
    {
      if (_tail[start - 1] == '\n' && ++count == lines) break;
      start--;
    }

    if (_tail.size() - start > maxBytes)
    {
      start = _tail.size() - maxBytes;
      while (start < _tail.size() && IsContinuation(_tail[start])) start++;
    }

    return { _tailStart + start, _tail.substr(start), _tailStart + _tail.size(), false };
  }

  optional<OutputChunk> ProcessOutput::Find(string_view text, uint64_t from) const
  {
    if (text.empty()) return nullopt;

    lock_guard lock{ _mutex };
    auto start = from > _tailStart ? size_t(min(from, _size) - _tailStart) : 0;
    auto match = search(_tail.begin() + ptrdiff_t(start), _tail.end(), text.begin(), text.end(), [](char left, char right) { return Lower(left) == Lower(right); });
    if (match == _tail.end()) return nullopt;

    auto position = size_t(match - _tail.begin());
    auto lineStart = position == 0 ? 0 : _tail.rfind('\n', position - 1);
    lineStart = lineStart == string::npos || position == 0 ? 0 : lineStart + 1;
    auto lineEnd = _tail.find('\n', position + text.size());
    auto next = lineEnd == string::npos ? _tail.size() : lineEnd + 1;

    auto line = _tail.substr(lineStart, (lineEnd == string::npos ? _tail.size() : lineEnd) - lineStart);
    if (line.ends_with('\r')) line.pop_back();
    return OutputChunk{ _tailStart + lineStart, move(line), _tailStart + next, _tailStart + next < _size };
  }

  bool ProcessOutput::WaitForChange(uint64_t size, milliseconds timeout) const
  {
    unique_lock lock{ _mutex };
    return _changed.wait_for(lock, timeout, [&] { return _size > size || !_isRunning; });
  }

  void ProcessOutput::Write(string_view text)
  {
    auto offset = _size;
    while (!text.empty() && _isWritable)
    {
      if (!_stream.is_open())
      {
        auto file = PartFile(_parts.size());
        error_code error;
        filesystem::create_directories(file.parent_path(), error);
        _stream.open(file, ios::binary | ios::trunc);
        if (!_stream)
        {
          logger{ "ProcessOutput" }.log(log_severity::warning, "Could not write the output to {}; only its tail is kept.", winrt::to_string(file.wstring()));
          _isWritable = false;
          return;
        }

        _parts.push_back({ offset, move(file) });
        _partBytes = 0;
      }

      //A file ends between characters, unless a single one is larger than a part
      auto room = size_t(min<uint64_t>(_partSize - min(_partBytes, _partSize), text.size()));
      if (room < text.size()) room = BackToBoundary(text, room);
      if (room == 0 && _partBytes == 0) room = text.size();

      _stream.write(text.data(), streamsize(room));
      _partBytes += room;
      offset += room;
      text.remove_prefix(room);
      if (!text.empty()) _stream.close();
    }

    if (_stream.is_open()) _stream.flush();
  }

  filesystem::path ProcessOutput::PartFile(size_t index) const
  {
    if (index == 0) return _file;
    return _file.parent_path() / format(L"{}.{}{}", _file.stem().wstring(), index + 1, _file.extension().wstring());
  }

  string ProcessOutput::ReadFiles(uint64_t from, size_t maxBytes) const
  {
    auto part = upper_bound(_parts.begin(), _parts.end(), from, [](uint64_t offset, const Part& item) { return offset < item.Start; });
    if (part == _parts.begin()) return {};
    part--;

    string text;
    for (; part != _parts.end() && text.size() < maxBytes; part++)
    {
      ifstream stream{ part->File, ios::binary };
      if (!stream) break;

      auto start = max(from + text.size(), part->Start) - part->Start;
      stream.seekg(streamoff(start));
      auto wanted = maxBytes - text.size();
      if (auto next = part + 1; next != _parts.end()) wanted = size_t(min<uint64_t>(wanted, next->Start - part->Start - start));

      string buffer(wanted, '\0');
      stream.read(buffer.data(), streamsize(wanted));
      buffer.resize(size_t(stream.gcount()));
      text += buffer;
      if (buffer.size() < wanted) break;
    }
    return text;
  }
#pragma endregion
}
