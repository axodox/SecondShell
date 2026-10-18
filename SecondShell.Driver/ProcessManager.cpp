#include "pch.h"
#include "ProcessManager.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Threading;
using namespace SecondShell::Actions;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Desktop
{
  const size_t _streamLimit = 64 * 1024;
  const milliseconds _pollInterval{ 50 };
  const milliseconds _cancelInterval{ 10 };
  const milliseconds _killTimeout{ 5000 };
  const milliseconds _cpuSampleInterval{ 250 };
  const milliseconds _ctrlCTimeout{ 1000 };

  //A program started by a shell may inherit the streams and keep them open after the shell exited, so the rest of the
  //output is awaited only this long
  const milliseconds _outputGrace{ 500 };

  namespace
  {
    //Ctrl+C reaches every process attached to the console, so this one too while it sends it. A handler function, as
    //ignoring Ctrl+C with a null handler would be inherited by the processes started here. Only handlers registered
    //while attached are called, and only until the process detaches, so it detaches once its own Ctrl+C has arrived
    //(Findings.md 2.16).
    manual_reset_event _ctrlCArrived;

    BOOL WINAPI IgnoreCtrlC(DWORD type)
    {
      if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT) return FALSE;

      _ctrlCArrived.set();
      return TRUE;
    }

    string ErrorMessage(DWORD error)
    {
      return winrt::to_string(winrt::hresult_error(HRESULT_FROM_WIN32(error)).message());
    }

    bool IsUtf8(string_view text)
    {
      return text.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0) > 0;
    }

    //PowerShell writes UTF-8 (see ShellCommandBuilder), most console programs the OEM code page
    string DecodeOutput(string_view bytes)
    {
      if (bytes.starts_with("\xEF\xBB\xBF")) bytes.remove_prefix(3);

      //The limit may have cut a character in two
      for (size_t trimmed = 0; trimmed < 4 && trimmed <= bytes.size(); trimmed++)
      {
        auto text = bytes.substr(0, bytes.size() - trimmed);
        if (IsUtf8(text)) return string(text);
      }

      wstring utf16(size_t(MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), int(bytes.size()), nullptr, 0)), L'\0');
      MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), int(bytes.size()), utf16.data(), int(utf16.size()));
      return winrt::to_string(utf16);
    }

    filesystem::path ProfileFolder()
    {
      wchar_t folder[MAX_PATH];
      auto length = GetEnvironmentVariableW(L"USERPROFILE", folder, MAX_PATH);
      if (length == 0 || length >= MAX_PATH) throw runtime_error("The user's profile folder is not known.");
      return folder;
    }

    filesystem::path WorkingDirectory(const RunOptions& options)
    {
      auto folder = options.WorkingDirectory ? filesystem::path(winrt::to_hstring(*options.WorkingDirectory).c_str()) : ProfileFolder();
      if (!filesystem::is_directory(folder)) throw invalid_argument(format("The working directory {} does not exist.", folder.string()));
      return folder;
    }

    bool WaitForExit(HANDLE process, milliseconds timeout, const CancellationToken& cancellation)
    {
      auto deadline = steady_clock::now() + timeout;
      while (WaitForSingleObject(process, DWORD(_pollInterval.count())) == WAIT_TIMEOUT)
      {
        if (cancellation.IsCancelled() || steady_clock::now() >= deadline) return false;
      }
      return true;
    }

    uint64_t ToTicks(const FILETIME& time)
    {
      return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    }

    //One of a process's output streams, read on a thread of its own; keeps the first _streamLimit bytes.
    class StreamCollector
    {
    public:
      explicit StreamCollector(winrt::file_handle&& pipe) :
        _pipe(move(pipe)),
        _thread([this] { Collect(); })
      { }

      ~StreamCollector()
      {
        Finish(milliseconds(0));
      }

      StreamCollector(const StreamCollector&) = delete;
      StreamCollector& operator=(const StreamCollector&) = delete;

      //Waits up to the grace period for the stream to end, then stops reading. Not thread-safe.
      void Finish(milliseconds grace)
      {
        if (!_thread.joinable()) return;

        //A zero timeout would wait forever
        if (grace > 0ms) _ended.wait(grace);

        _isStopping = true;
        while (!_ended)
        {
          CancelSynchronousIo(_thread.native_handle());
          this_thread::sleep_for(_cancelInterval);
        }
        _thread.join();
      }

      //After Finish.
      string Text() const
      {
        return DecodeOutput(_bytes);
      }

      bool IsTruncated() const
      {
        return _isTruncated;
      }

    private:
      winrt::file_handle _pipe;
      string _bytes;
      bool _isTruncated = false;
      atomic_bool _isStopping = false;
      manual_reset_event _ended;
      thread _thread;

      void Collect()
      {
        char buffer[4096];
        DWORD read = 0;
        while (!_isStopping && ReadFile(_pipe.get(), buffer, sizeof(buffer), &read, nullptr) && read > 0)
        {
          auto kept = min(size_t(read), _streamLimit - _bytes.size());
          _bytes.append(buffer, kept);
          if (kept < read) _isTruncated = true;
        }
        _ended.set();
      }
    };

    struct ProcessSample
    {
      ProcessInfo Info;
      optional<uint64_t> CpuTicks;
    };
  }

  class ProcessManager::StartedProcess
  {
  public:
    StartedProcess(winrt::handle&& process, winrt::file_handle&& output, winrt::file_handle&& error) :
      Process(move(process)),
      Output(move(output)),
      Error(move(error))
    { }

    winrt::handle Process;
    StreamCollector Output;
    StreamCollector Error;
  };

  ProcessManager::ProcessManager(dependency_container* container) :
    _commands(container->resolve<ShellCommandBuilder>())
  {
    DWORD sessionId = 0;
    winrt::check_bool(ProcessIdToSessionId(GetCurrentProcessId(), &sessionId));
    _sessionId = sessionId;
  }

  uint32_t ProcessManager::Start(const RunOptions& options)
  {
    auto commandLine = _commands->Build(options.Shell, options.Command, options.Args);
    auto workingDirectory = WorkingDirectory(options);

    //Standard input is empty, so a process reading it does not wait forever
    SECURITY_ATTRIBUTES inheritable{ .nLength = sizeof(SECURITY_ATTRIBUTES), .bInheritHandle = TRUE };
    winrt::file_handle input{ CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0, nullptr) };
    winrt::file_handle outputRead, outputWrite, errorRead, errorWrite;
    winrt::check_bool(CreatePipe(outputRead.put(), outputWrite.put(), &inheritable, 0));
    winrt::check_bool(CreatePipe(errorRead.put(), errorWrite.put(), &inheritable, 0));
    winrt::check_bool(SetHandleInformation(outputRead.get(), HANDLE_FLAG_INHERIT, 0));
    winrt::check_bool(SetHandleInformation(errorRead.get(), HANDLE_FLAG_INHERIT, 0));

    //Only these are inherited: a process started at the same time on another thread must not get our pipes
    HANDLE inherited[] = { input.get(), outputWrite.get(), errorWrite.get() };
    SIZE_T attributesSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributesSize);
    vector<uint8_t> attributesBuffer(attributesSize);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributesBuffer.data());
    winrt::check_bool(InitializeProcThreadAttributeList(attributes, 1, 0, &attributesSize));
    winrt::check_bool(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr));

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.get();
    startup.StartupInfo.hStdOutput = outputWrite.get();
    startup.StartupInfo.hStdError = errorWrite.get();
    startup.lpAttributeList = attributes;

    //Console programs get a console of their own, without a window
    PROCESS_INFORMATION information{};
    auto isStarted = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, workingDirectory.c_str(), &startup.StartupInfo, &information);
    auto error = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    if (!isStarted) throw runtime_error(format("The process could not start: {}", ErrorMessage(error)));

    CloseHandle(information.hThread);

    //Without our copies of the write ends, the streams end when the process, and whatever it passed them to, exits
    outputWrite.close();
    errorWrite.close();

    auto started = make_shared<StartedProcess>(winrt::handle{ information.hProcess }, move(outputRead), move(errorRead));
    {
      lock_guard lock{ _mutex };
      _started[information.dwProcessId] = move(started);
    }

    _logger.log(log_severity::information, "Started process {}.", information.dwProcessId);
    return information.dwProcessId;
  }

  uint32_t ProcessManager::Launch(const RunOptions& options)
  {
    auto commandLine = _commands->Build(options.Shell, options.Command, options.Args);
    auto workingDirectory = WorkingDirectory(options);

    STARTUPINFOW startup{ .cb = sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION information{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, workingDirectory.c_str(), &startup, &information))
    {
      throw runtime_error(format("The process could not start: {}", ErrorMessage(GetLastError())));
    }

    CloseHandle(information.hThread);
    CloseHandle(information.hProcess);

    _logger.log(log_severity::information, "Launched process {}.", information.dwProcessId);
    return information.dwProcessId;
  }

  optional<ProcessResult> ProcessManager::Wait(uint32_t processId, milliseconds timeout, const CancellationToken& cancellation)
  {
    shared_ptr<StartedProcess> started;
    {
      lock_guard lock{ _mutex };
      if (auto item = _started.find(processId); item != _started.end()) started = item->second;
    }

    winrt::handle opened;
    if (!started) opened = Open(processId, SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION);
    auto process = started ? started->Process.get() : opened.get();
    if (!WaitForExit(process, timeout, cancellation)) return nullopt;

    ProcessResult result;
    result.ProcessId = processId;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(process, &exitCode)) result.ExitCode = int32_t(exitCode);

    //The output goes to whoever waits first
    {
      lock_guard lock{ _mutex };
      if (started && !_started.erase(processId)) started.reset();
    }

    if (started)
    {
      started->Output.Finish(_outputGrace);
      started->Error.Finish(_outputGrace);
      result.StandardOutput = started->Output.Text();
      result.StandardError = started->Error.Text();
      result.IsOutputTruncated = started->Output.IsTruncated();
      result.IsErrorTruncated = started->Error.IsTruncated();
    }

    return result;
  }

  optional<ProcessResult> ProcessManager::Stop(uint32_t processId, milliseconds timeout, const CancellationToken& cancellation)
  {
    Open(processId, SYNCHRONIZE);

    //Ctrl+C first: closing a console sends a close event to every process attached to it, which then terminates, so this
    //must not be attached any more when its window closes
    SendCtrlC(processId);

    //Its windows, as clicking their X button would
    EnumWindows([](HWND window, LPARAM context) {
      DWORD windowProcessId = 0;
      GetWindowThreadProcessId(window, &windowProcessId);
      if (windowProcessId == DWORD(context) && IsWindowVisible(window) && !GetWindow(window, GW_OWNER)) PostMessageW(window, WM_SYSCOMMAND, SC_CLOSE, 0);
      return TRUE;
      }, LPARAM(processId));

    return Wait(processId, timeout, cancellation);
  }

  void ProcessManager::Kill(uint32_t processId, bool includeDescendants)
  {
    if (processId == GetCurrentProcessId()) throw invalid_argument(format("Process {} is the SecondShell driver.", processId));

    vector<winrt::handle> processes;
    processes.push_back(Open(processId, PROCESS_TERMINATE | SYNCHRONIZE));
    if (includeDescendants)
    {
      for (auto descendant : Descendants(processId))
      {
        if (winrt::handle process{ OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, descendant) }) processes.push_back(move(process));
      }
    }

    for (auto& process : processes) TerminateProcess(process.get(), 1);
    for (auto& process : processes) WaitForSingleObject(process.get(), DWORD(_killTimeout.count()));
  }

  vector<ProcessInfo> ProcessManager::Enumerate() const
  {
    //The processes of this session, sampled twice for their CPU use
    auto takeSample = [this] {
      unordered_map<uint32_t, ProcessSample> samples;
      winrt::handle snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
      PROCESSENTRY32W entry{ .dwSize = sizeof(PROCESSENTRY32W) };
      for (auto found = Process32FirstW(snapshot.get(), &entry); found; found = Process32NextW(snapshot.get(), &entry))
      {
        DWORD sessionId = 0;
        if (!ProcessIdToSessionId(entry.th32ProcessID, &sessionId) || sessionId != _sessionId) continue;

        ProcessSample sample;
        sample.Info.ProcessId = entry.th32ProcessID;
        sample.Info.ParentProcessId = entry.th32ParentProcessID;
        sample.Info.Name = winrt::to_string(entry.szExeFile);

        if (winrt::handle process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID) })
        {
          wchar_t path[MAX_PATH];
          DWORD size = MAX_PATH;
          if (QueryFullProcessImageNameW(process.get(), 0, path, &size)) sample.Info.ExecutablePath = winrt::to_string(path);

          PROCESS_MEMORY_COUNTERS memory{ .cb = sizeof(PROCESS_MEMORY_COUNTERS) };
          if (K32GetProcessMemoryInfo(process.get(), &memory, sizeof(memory))) sample.Info.MemoryBytes = memory.WorkingSetSize;

          FILETIME creation, exit, kernel, user;
          if (GetProcessTimes(process.get(), &creation, &exit, &kernel, &user)) sample.CpuTicks = ToTicks(kernel) + ToTicks(user);
        }

        samples[entry.th32ProcessID] = move(sample);
      }
      return samples;
      };

    auto first = takeSample();
    auto start = steady_clock::now();
    this_thread::sleep_for(_cpuSampleInterval);
    auto second = takeSample();
    auto elapsedTicks = double(duration_cast<nanoseconds>(steady_clock::now() - start).count()) / 100.0;

    SYSTEM_INFO system;
    GetSystemInfo(&system);

    vector<ProcessInfo> processes;
    for (auto& [processId, current] : second)
    {
      auto previous = first.find(processId);
      if (current.CpuTicks && previous != first.end() && previous->second.CpuTicks)
      {
        current.Info.CpuPercent = double(*current.CpuTicks - *previous->second.CpuTicks) / (elapsedTicks * system.dwNumberOfProcessors) * 100.0;
      }
      processes.push_back(move(current.Info));
    }

    ranges::sort(processes, {}, &ProcessInfo::ProcessId);
    return processes;
  }

  winrt::handle ProcessManager::Open(uint32_t processId, DWORD access) const
  {
    DWORD sessionId = 0;
    if (!ProcessIdToSessionId(processId, &sessionId)) throw invalid_argument(format("There is no process {}.", processId));
    if (sessionId != _sessionId) throw invalid_argument(format("Process {} is not in the session.", processId));

    winrt::handle process{ OpenProcess(access, FALSE, processId) };
    if (!process) throw invalid_argument(format("Process {} cannot be accessed: {}", processId, ErrorMessage(GetLastError())));
    return process;
  }

  vector<uint32_t> ProcessManager::Descendants(uint32_t processId) const
  {
    //A child's creation time follows its parent's; otherwise the parent's ID was reused, and it is not a descendant
    auto creationTime = [](uint32_t id) -> optional<uint64_t> {
      winrt::handle process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id) };
      FILETIME creation, exit, kernel, user;
      if (!process || !GetProcessTimes(process.get(), &creation, &exit, &kernel, &user)) return nullopt;
      return ToTicks(creation);
      };

    unordered_multimap<uint32_t, uint32_t> children;
    winrt::handle snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
    PROCESSENTRY32W entry{ .dwSize = sizeof(PROCESSENTRY32W) };
    for (auto found = Process32FirstW(snapshot.get(), &entry); found; found = Process32NextW(snapshot.get(), &entry))
    {
      if (entry.th32ProcessID != entry.th32ParentProcessID) children.emplace(entry.th32ParentProcessID, entry.th32ProcessID);
    }

    vector<uint32_t> descendants;
    vector<pair<uint32_t, uint64_t>> parents;
    if (auto created = creationTime(processId)) parents.emplace_back(processId, *created);
    while (!parents.empty())
    {
      auto [parent, parentCreated] = parents.back();
      parents.pop_back();

      auto range = children.equal_range(parent);
      for (auto child = range.first; child != range.second; child++)
      {
        auto childCreated = creationTime(child->second);
        if (!childCreated || *childCreated < parentCreated || child->second == GetCurrentProcessId()) continue;

        descendants.push_back(child->second);
        parents.emplace_back(child->second, *childCreated);
      }
    }
    return descendants;
  }

  void ProcessManager::SendCtrlC(uint32_t processId)
  {
    lock_guard lock{ _consoleMutex };
    if (!AttachConsole(processId)) return;

    //Once only, however often this attaches
    SetConsoleCtrlHandler(&IgnoreCtrlC, FALSE);
    SetConsoleCtrlHandler(&IgnoreCtrlC, TRUE);

    //The console delivers it on a thread of its own, also to this process
    _ctrlCArrived.reset();
    if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0)) _ctrlCArrived.wait(_ctrlCTimeout);
    FreeConsole();
  }
}
