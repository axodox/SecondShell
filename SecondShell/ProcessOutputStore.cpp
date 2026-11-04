#include "pch.h"
#include "ProcessOutputStore.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Protocol;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Agent
{
  ProcessOutputStore::ProcessOutputStore(dependency_container* container) :
    _driver(container->resolve<DriverLink>()),
    _settings(container->resolve<SettingsStore>())
  {
    _outputSubscription = _driver->ProcessOutput([this](DriverLink*, const ProcessOutputNotification& output) { Add(output); });
    _acceptedSubscription = _driver->Accepted([this](DriverLink*) { Configure(); });
    _settingsSubscription = _settings->Changed([this](SettingsStore*, const Settings&) { Configure(); });
  }

  void ProcessOutputStore::Configure()
  {
    _driver->ConfigureOutput({ _settings->Current().OutputPartSize });
  }

  void ProcessOutputStore::Add(const ProcessOutputNotification& output)
  {
    {
      lock_guard lock{ _mutex };
      //The newest of the ID, unless it ended and this is a new process which got the same ID, starting its output
      auto found = ranges::find_if(_entries | views::reverse, [&](const Entry& item) { return item.Info.ProcessId == output.ProcessId; });
      auto isNew = found == (_entries | views::reverse).end() || (!found->Info.IsRunning && output.Offset == 0 && found->Next > 0);

      Entry* entry;
      if (isNew)
      {
        _entries.push_back({ .Info = { output.ProcessId, output.Command, output.OutputFile, true, nullopt, system_clock::now() } });
        if (_entries.size() > MaxProcesses) _entries.pop_front();
        entry = &_entries.back();
      }
      else
      {
        entry = &*found;
      }

      //The driver keeps only the tail in memory: what the view missed is in the file
      if (output.Offset > entry->Next) AppendText(*entry, format("{}[{} KB more in the output file]\n", entry->IsLastOpen ? "\n" : "", (output.Offset - entry->Next + 1023) / 1024));
      if (output.Offset + output.Text.size() > entry->Next)
      {
        auto skipped = size_t(entry->Next > output.Offset ? entry->Next - output.Offset : 0);
        AppendText(*entry, string_view(output.Text).substr(skipped));
        entry->Next = output.Offset + output.Text.size();
      }

      entry->Info.IsRunning = output.IsRunning;
      entry->Info.ExitCode = output.ExitCode;
    }
    _version++;
  }

  void ProcessOutputStore::AppendText(Entry& entry, string_view text)
  {
    while (!text.empty())
    {
      auto end = text.find('\n');
      auto line = text.substr(0, end);
      if (line.ends_with('\r')) line.remove_suffix(1);

      if (entry.IsLastOpen) entry.Lines.back() += line;
      else
      {
        entry.Lines.emplace_back(line);
        entry.Info.LineCount++;
      }

      entry.IsLastOpen = end == string_view::npos;
      text = end == string_view::npos ? string_view{} : text.substr(end + 1);
    }

    while (entry.Lines.size() > MaxLines)
    {
      entry.Lines.pop_front();
      entry.FirstLine++;
    }
  }

  vector<ProcessOutputInfo> ProcessOutputStore::Processes() const
  {
    lock_guard lock{ _mutex };
    vector<ProcessOutputInfo> result;
    for (auto& entry : _entries | views::reverse) result.push_back(entry.Info);
    return result;
  }

  ProcessOutputLines ProcessOutputStore::Lines(uint32_t processId, uint64_t from) const
  {
    lock_guard lock{ _mutex };

    //The newest process of the ID
    auto entry = ranges::find_if(_entries | views::reverse, [&](const Entry& item) { return item.Info.ProcessId == processId; });
    if (entry == (_entries | views::reverse).end()) return {};

    auto first = max(from, entry->FirstLine);
    auto start = size_t(min<uint64_t>(first - entry->FirstLine, entry->Lines.size()));
    return { entry->FirstLine, first, { entry->Lines.begin() + ptrdiff_t(start), entry->Lines.end() } };
  }

  uint64_t ProcessOutputStore::Version() const
  {
    return _version;
  }

  namespace
  {
    //"20261104-203238-21856-npm" or, for a later part, "20261104-203238-21856-npm.2": the driver's names (UTC)
    optional<pair<ProcessOutputFile, uint32_t>> ParseOutputName(const filesystem::path& file)
    {
      auto stem = file.stem().string();
      uint32_t part = 1;
      if (auto dot = stem.rfind('.'); dot != string::npos)
      {
        if (from_chars(stem.data() + dot + 1, stem.data() + stem.size(), part).ec != errc{}) return nullopt;
        stem.resize(dot);
      }

      int year, month, day, hour, minute, second;
      uint32_t processId = 0;
      char program[128] = {};
      if (sscanf_s(stem.c_str(), "%4d%2d%2d-%2d%2d%2d-%u-%127s", &year, &month, &day, &hour, &minute, &second, &processId, program, unsigned(sizeof(program))) != 8) return nullopt;

      ProcessOutputFile result;
      result.ProcessId = processId;
      result.Program = program;
      result.Started = sys_days{ chrono::year{ year } / month / day } + hours{ hour } + minutes{ minute } + seconds{ second };
      result.File = file.parent_path() / (stem + file.extension().string());
      return pair{ move(result), part };
    }
  }

  vector<ProcessOutputFile> ProcessOutputStore::History(const filesystem::path& folder, size_t maxCount)
  {
    map<filesystem::path, ProcessOutputFile> files;
    map<filesystem::path, vector<pair<uint32_t, filesystem::path>>> parts;
    error_code error;
    for (auto& entry : filesystem::directory_iterator(folder, error))
    {
      if (!entry.is_regular_file(error) || entry.path().extension() != ".log") continue;
      auto parsed = ParseOutputName(entry.path());
      if (!parsed) continue;

      auto& [info, part] = *parsed;
      auto& file = files.try_emplace(info.File, info).first->second;
      file.Size += entry.file_size(error);
      parts[info.File].emplace_back(part, entry.path());
    }

    vector<ProcessOutputFile> result;
    for (auto& [path, file] : files)
    {
      auto& fileParts = parts[path];
      ranges::sort(fileParts);
      for (auto& [number, part] : fileParts) file.Parts.push_back(part);
      result.push_back(move(file));
    }

    ranges::sort(result, greater{}, &ProcessOutputFile::Started);
    if (result.size() > maxCount) result.resize(maxCount);
    return result;
  }

  vector<string> ProcessOutputStore::ReadTail(const ProcessOutputFile& file, size_t maxBytes, size_t maxLines)
  {
    //From the last parts back, until there is enough
    string text;
    for (auto part = file.Parts.rbegin(); part != file.Parts.rend() && text.size() < maxBytes; part++)
    {
      ifstream stream{ *part, ios::binary | ios::ate };
      if (!stream) continue;

      auto size = uint64_t(stream.tellg());
      auto wanted = min<uint64_t>(size, maxBytes - text.size());
      stream.seekg(streamoff(size - wanted));
      string buffer(size_t(wanted), '\0');
      stream.read(buffer.data(), streamsize(wanted));
      buffer.resize(size_t(stream.gcount()));
      text = buffer + text;
    }

    //Whole lines only, unless it is all of it
    vector<string> lines;
    string_view rest = text;
    auto total = file.Size;
    if (text.size() < total)
    {
      auto newline = rest.find('\n');
      rest = newline == string_view::npos ? string_view{} : rest.substr(newline + 1);
    }

    while (!rest.empty())
    {
      auto end = rest.find('\n');
      auto line = rest.substr(0, end);
      if (line.ends_with('\r')) line.remove_suffix(1);
      lines.emplace_back(line);
      rest = end == string_view::npos ? string_view{} : rest.substr(end + 1);
    }

    if (lines.size() > maxLines) lines.erase(lines.begin(), lines.end() - ptrdiff_t(maxLines));
    return lines;
  }
}
