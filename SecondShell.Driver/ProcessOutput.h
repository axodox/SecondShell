#pragma once
#include <fstream>

namespace SecondShell::Desktop
{
  //Turns the bytes of an output stream into UTF-8 text as they arrive: as UTF-8 while they are valid, else in the
  //console's OEM code page from then on. A character split between reads waits for its rest.
  class OutputDecoder
  {
  public:
    std::string Decode(std::string_view bytes);

    //What is left once the stream ended.
    std::string Finish();

  private:
    std::string _pending;
    bool _isOem = false;
    bool _isStart = true;
  };

  //Part of an output, by offsets: bytes of the UTF-8 text from the start of the output.
  struct OutputChunk
  {
    uint64_t From = 0;
    std::string Text;
    uint64_t Next = 0;
    bool IsTruncated = false;  //more output follows before Next
  };

  //The output of one process started with output: both of its streams as they arrive. Its tail stays in memory, and
  //all of it goes to files of at most a part size each (the first file's name, then .2, .3 before .log), so a verbose
  //program fills files rather than memory, and older output can still be read. Thread-safe.
  class ProcessOutput
  {
  public:
    ProcessOutput(std::filesystem::path file, uint64_t partSize, size_t tailSize);

    void Append(std::string_view text);

    //The process exited; output may still follow, from programs it passed its streams to.
    void End(std::optional<int32_t> exitCode);

    const std::filesystem::path& File() const;
    uint64_t Size() const;
    uint64_t TailStart() const;
    bool IsRunning() const;
    std::optional<int32_t> ExitCode() const;

    //From an offset on: from memory, or from the files for older output.
    OutputChunk Read(uint64_t from, size_t maxBytes) const;

    //The last lines.
    OutputChunk Tail(uint32_t lines, size_t maxBytes) const;

    //The first line from an offset on which contains the text, ignoring case, as far as the tail reaches.
    std::optional<OutputChunk> Find(std::string_view text, uint64_t from) const;

    //Until the output grows beyond a size or the process ends; false if neither happened within the timeout.
    bool WaitForChange(uint64_t size, std::chrono::milliseconds timeout) const;

  private:
    struct Part
    {
      uint64_t Start = 0;
      std::filesystem::path File;
    };

    std::filesystem::path _file;
    uint64_t _partSize;
    size_t _tailSize;

    mutable std::mutex _mutex;
    mutable std::condition_variable _changed;
    std::vector<Part> _parts;
    std::ofstream _stream;
    uint64_t _partBytes = 0;
    bool _isWritable = true;  //false once a file could not be written: the tail goes on
    std::string _tail;
    uint64_t _tailStart = 0;
    uint64_t _size = 0;
    bool _isRunning = true;
    std::optional<int32_t> _exitCode;

    void Write(std::string_view text);
    std::filesystem::path PartFile(size_t index) const;
    std::string ReadFiles(uint64_t from, size_t maxBytes) const;
  };
}
