#pragma once
//Shared helpers of the stage 1 spikes: the exchange folder, logging, key-value files and the session guard.
//Throwaway code, not product code.
#include <windows.h>
#include <shlobj.h>
#include <wtsapi32.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace Spike
{
  //Per-frame data the shared frames writer (child session) passes to the reader (host) in a shared section, written under the keyed mutex.
  struct SharedFrameMeta
  {
    uint64_t FrameIndex;      //1-based index of the captured frame
    int64_t CaptureTime;      //Direct3D11CaptureFrame.SystemRelativeTime (QPC, 100 ns units)
    int64_t ArrivedTime;      //QPC (100 ns units) when FrameArrived was raised in the writer
    int64_t WriteTime;        //QPC (100 ns units) when the writer released the texture to the reader
    uint64_t WriterSkipped;   //frames the writer skipped because the reader still held the texture
    uint32_t Width, Height;
  };

  //QueryPerformanceCounter in 100 ns units: the same clock as SystemRelativeTime, comparable across sessions.
  inline int64_t QpcNow100ns()
  {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return int64_t(counter.QuadPart / frequency.QuadPart * 10'000'000 + counter.QuadPart % frequency.QuadPart * 10'000'000 / frequency.QuadPart);
  }

  //The exchange folder of the spikes: Spikes\exchange next to Spikes\bin, found from this executable's path.
  //Not under the user profile: a process started from a packaged app (e.g. the Claude desktop app) has its
  //AppData and HKCU writes redirected into the package, so they would be invisible to the child session.
  inline std::filesystem::path OutDir()
  {
    wchar_t module[MAX_PATH];
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::filesystem::path result;
    for (auto path = std::filesystem::path(module).parent_path(); path.has_relative_path(); path = path.parent_path())
    {
      if (_wcsicmp(path.filename().c_str(), L"bin") == 0)
      {
        result = path.parent_path() / L"exchange";
        break;
      }
    }

    if (result.empty())
    {
      PWSTR path = nullptr;
      SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
      result = std::filesystem::path(path) / L"SecondShell.Spikes";
      CoTaskMemFree(path);
    }
    std::filesystem::create_directories(result);
    return result;
  }

  inline std::string Timestamp()
  {
    SYSTEMTIME t;
    GetLocalTime(&t);
    return std::format("{:02}:{:02}:{:02}.{:03}", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  }

  inline uint32_t CurrentSessionId()
  {
    DWORD id = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &id);
    return id;
  }

  class Logger
  {
    std::mutex _mutex;
    std::filesystem::path _path;

  public:
    explicit Logger(std::filesystem::path path) : _path(std::move(path)) {}

    void operator()(const std::string& message)
    {
      std::lock_guard lock(_mutex);
      std::ofstream file(_path, std::ios::app | std::ios::binary);
      file << Timestamp() << " " << message << "\n";
      OutputDebugStringA((message + "\n").c_str());
    }

    const std::filesystem::path& Path() const { return _path; }
  };

  inline std::string ReadAll(const std::filesystem::path& path)
  {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  //Writes through a temporary file, so the reader never sees a half-written file.
  inline void WriteAll(const std::filesystem::path& path, const std::string& text)
  {
    auto temp = path;
    temp += L".tmp";
    {
      std::ofstream file(temp, std::ios::binary | std::ios::trunc);
      file << text;
    }
    MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
  }

  inline std::map<std::string, std::string> ReadKeyValues(const std::filesystem::path& path)
  {
    std::map<std::string, std::string> result;
    std::istringstream lines(ReadAll(path));
    std::string line;
    while (std::getline(lines, line))
    {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      auto separator = line.find('=');
      if (separator != std::string::npos) result[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return result;
  }

  inline std::string Narrow(std::wstring_view text)
  {
    if (text.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), result.data(), length, nullptr, nullptr);
    return result;
  }

  //Session guard: returns only if this process runs in the child session the host expects
  //(expected_session.txt, written by the host after sign-in) and not in the console session. Otherwise exits.
  inline void SessionGuard(Logger& log)
  {
    auto expectedPath = OutDir() / L"expected_session.txt";
    auto session = CurrentSessionId();

    std::optional<uint32_t> expected;
    for (int i = 0; i < 40 && !expected; i++)
    {
      auto text = ReadAll(expectedPath);
      if (!text.empty()) expected = uint32_t(std::stoul(text));
      else std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    auto console = WTSGetActiveConsoleSessionId();
    if (!expected || *expected != session || session == console)
    {
      log(std::format("GUARD: refusing to run in session {} (expected {}, console {})", session, expected ? std::to_string(*expected) : "missing", console));
      ExitProcess(1);
    }

    log(std::format("START session={} (console {}) pid={}", session, console, GetCurrentProcessId()));
  }

  inline bool HasArg(int argc, wchar_t** argv, std::wstring_view name)
  {
    for (int i = 1; i < argc; i++) if (argv[i] == name) return true;
    return false;
  }

  inline std::wstring ArgValue(int argc, wchar_t** argv, std::wstring_view name, std::wstring_view fallback = {})
  {
    for (int i = 1; i + 1 < argc; i++) if (argv[i] == name) return argv[i + 1];
    return std::wstring(fallback);
  }
}

