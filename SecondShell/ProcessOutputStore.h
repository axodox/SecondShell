#pragma once
#include "DriverLink.h"
#include "SettingsStore.h"

namespace SecondShell::Agent
{
  //A process the agent started with output, as the Processes flyout lists it.
  struct ProcessOutputInfo
  {
    uint32_t ProcessId = 0;
    std::string Command;
    std::string OutputFile;
    bool IsRunning = true;
    std::optional<int32_t> ExitCode;
    std::chrono::system_clock::time_point Started;
    uint64_t LineCount = 0;  //lines so far, the dropped ones included
  };

  //An output file in the output folder, its parts together: of a process of this run or an earlier one.
  struct ProcessOutputFile
  {
    std::filesystem::path File;               //the first part, as the process reported it
    std::vector<std::filesystem::path> Parts;  //in order
    uint32_t ProcessId = 0;
    std::string Program;
    std::chrono::system_clock::time_point Started;
    uint64_t Size = 0;
  };

  //Lines of a process's output, numbered from the first line of its output.
  struct ProcessOutputLines
  {
    uint64_t Oldest = 0;  //the first line still kept
    uint64_t First = 0;
    std::vector<std::string> Lines;  //the last one may still grow
  };

  //The output of the processes the agent started with output, as the driver sends it (Design.md, Process output): the
  //last lines of each, for the Processes flyout, which shows them live; all of it is in the driver's files. Also passes
  //the user's part size on to the driver. Thread-safe.
  class ProcessOutputStore
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ProcessOutputStore" };

  public:
    static constexpr size_t MaxLines = 10000;
    static constexpr size_t MaxProcesses = 20;

    //Tests: not connected to the driver. Not a default constructor, which the container would prefer.
    explicit ProcessOutputStore(std::nullptr_t) { }
    explicit ProcessOutputStore(Axodox::Infrastructure::dependency_container* container);

    void Add(const Protocol::ProcessOutputNotification& output);

    //Newest first.
    std::vector<ProcessOutputInfo> Processes() const;

    //From a line on, as far as it is kept; empty for an unknown process.
    ProcessOutputLines Lines(uint32_t processId, uint64_t from) const;

    //Changes with every addition, so a view can tell whether to look again.
    uint64_t Version() const;

    //The output files in a folder, newest first, at most this many: also of earlier runs (Design.md, Process output).
    static std::vector<ProcessOutputFile> History(const std::filesystem::path& folder, size_t maxCount = 100);

    //The last lines of an output file's parts, from at most its last bytes.
    static std::vector<std::string> ReadTail(const ProcessOutputFile& file, size_t maxBytes = 4 * 1024 * 1024, size_t maxLines = MaxLines);

  private:
    struct Entry
    {
      ProcessOutputInfo Info;
      std::deque<std::string> Lines;
      uint64_t FirstLine = 0;
      bool IsLastOpen = false;  //the last line has no line break yet
      uint64_t Next = 0;        //the output's offset after what arrived
    };

    std::shared_ptr<Session::DriverLink> _driver;
    std::shared_ptr<Session::SettingsStore> _settings;
    Axodox::Infrastructure::event_subscription _outputSubscription;
    Axodox::Infrastructure::event_subscription _acceptedSubscription;
    Axodox::Infrastructure::event_subscription _settingsSubscription;

    mutable std::mutex _mutex;
    std::deque<Entry> _entries;  //oldest first
    std::atomic_uint64_t _version = 0;

    void Configure();
    static void AppendText(Entry& entry, std::string_view text);
  };
}
