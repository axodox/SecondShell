#include "pch.h"
#include "CaptureStore.h"
#include "ProcessOutput.h"
#include "ProcessOutputStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Desktop;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  TEST_CLASS(ProcessOutputTests)
  {
    filesystem::path _folder = filesystem::temp_directory_path() / format("SecondShell.Tests.Output.{}", GetCurrentProcessId());

  public:
    ~ProcessOutputTests()
    {
      error_code error;
      filesystem::remove_all(_folder, error);
    }

    TEST_METHOD(ACharacterSplitBetweenReadsWaitsForItsRest)
    {
      OutputDecoder decoder;
      Assert::AreEqual(string("a"), decoder.Decode("a\xC3"), L"the first byte of \xC3\xA9 waits");
      Assert::AreEqual(string("\xC3\xA9" "b"), decoder.Decode("\xA9" "b"));
      Assert::AreEqual(string(), decoder.Finish());
    }

    TEST_METHOD(AByteOrderMarkIsDropped)
    {
      OutputDecoder decoder;
      Assert::AreEqual(string(), decoder.Decode("\xEF\xBB"));
      Assert::AreEqual(string("x"), decoder.Decode("\xBFx"));
    }

    TEST_METHOD(OutputWhichIsNotUtf8IsReadInTheOemCodePage)
    {
      //0x82 is é in code page 850 and 437, and never valid on its own in UTF-8
      OutputDecoder decoder;
      auto text = decoder.Decode("caf\x82\n");
      Assert::IsTrue(text.starts_with("caf") && text.ends_with("\n") && text.size() > 5, L"decoded to UTF-8, not dropped");
      Assert::AreEqual(string("ok"), decoder.Decode("ok"), L"later reads are read the same way");
    }

    TEST_METHOD(TheTailAndTheLastLinesAreRead)
    {
      ProcessOutput output{ _folder / "tail.log", 1024 * 1024, 1024 };
      output.Append("one\ntwo\nthree\n");
      output.Append("four");

      auto tail = output.Tail(2, 1000);
      Assert::AreEqual(string("three\nfour"), tail.Text);
      Assert::AreEqual(uint64_t(8), tail.From);
      Assert::AreEqual(uint64_t(18), tail.Next);

      auto read = output.Read(4, 3);
      Assert::AreEqual(string("two"), read.Text);
      Assert::IsTrue(read.IsTruncated, L"more follows");
      Assert::AreEqual(uint64_t(7), read.Next);
    }

    TEST_METHOD(OutputGoesToFilesInPartsAndOldOutputIsReadFromThem)
    {
      //A tiny tail and parts, so most of it is only in the files
      ProcessOutput output{ _folder / "parts.log", 10, 8 };
      string all;
      for (auto index = 0; index < 10; index++)
      {
        auto line = format("line {}\n", index);
        output.Append(line);
        all += line;
      }

      Assert::IsTrue(filesystem::exists(_folder / "parts.log"));
      Assert::IsTrue(filesystem::exists(_folder / "parts.2.log"), L"the output continues in the next part");
      Assert::IsTrue(filesystem::file_size(_folder / "parts.log") <= 10);
      Assert::IsTrue(output.TailStart() > 0, L"the start is no longer in memory");

      auto read = output.Read(0, 1000);
      Assert::AreEqual(all, read.Text, L"read back from the parts");
      Assert::IsFalse(read.IsTruncated);
    }

    TEST_METHOD(AFileNeverEndsWithinACharacter)
    {
      ProcessOutput output{ _folder / "chars.log", 4, 1024 };
      output.Append("ab\xC3\xA9\xC3\xA9");

      Assert::AreEqual(uintmax_t(4), filesystem::file_size(_folder / "chars.log"));
      Assert::AreEqual(string("ab\xC3\xA9\xC3\xA9"), output.Read(0, 100).Text);
      Assert::AreEqual(string("\xC3\xA9"), output.Read(3, 100).Text, L"an offset within a character starts after it");
    }

    TEST_METHOD(ALineIsFoundIgnoringCase)
    {
      ProcessOutput output{ _folder / "find.log", 1024 * 1024, 1024 };
      output.Append("Starting\r\nServer LISTENING on 8080\r\nready");

      auto found = output.Find("listening on", 0);
      Assert::IsTrue(found.has_value());
      Assert::AreEqual(string("Server LISTENING on 8080"), found->Text, L"the whole line, without its line break");
      Assert::AreEqual(uint64_t(10), found->From);
      Assert::AreEqual(uint64_t(36), found->Next);

      Assert::IsFalse(output.Find("listening", found->Next).has_value(), L"from an offset on");
      Assert::IsFalse(output.Find("missing", 0).has_value());
    }

    TEST_METHOD(WaitsEndWithOutputOrTheExit)
    {
      ProcessOutput output{ _folder / "wait.log", 1024 * 1024, 1024 };
      Assert::IsFalse(output.WaitForChange(0, 20ms), L"nothing yet");

      thread writer([&] { this_thread::sleep_for(30ms); output.Append("x"); });
      Assert::IsTrue(output.WaitForChange(0, 5s));
      writer.join();

      output.End(3);
      Assert::IsTrue(output.WaitForChange(1, 5s), L"the end wakes it too");
      Assert::IsFalse(output.IsRunning());
      Assert::AreEqual(3, *output.ExitCode());
    }
  };

  TEST_CLASS(ProcessOutputStoreTests)
  {
    static Protocol::ProcessOutputNotification Output(uint32_t processId, uint64_t offset, string text, bool isRunning = true)
    {
      return { processId, "server.exe", "C:\\out.log", offset, move(text), isRunning, isRunning ? nullopt : optional<int32_t>(0) };
    }

  public:
    TEST_METHOD(OutputBecomesLinesAsItArrives)
    {
      Agent::ProcessOutputStore store{ nullptr };
      store.Add(Output(5, 0, "one\r\ntw"));
      store.Add(Output(5, 7, "o\nthree"));

      auto lines = store.Lines(5, 0);
      Assert::IsTrue(lines.Lines == vector<string>{ "one", "two", "three" }, L"a line split between notifications is one line, without its \\r");

      auto from = store.Lines(5, 2);
      Assert::AreEqual(uint64_t(2), from.First);
      Assert::IsTrue(from.Lines == vector<string>{ "three" });
    }

    TEST_METHOD(OutputTheViewMissedIsNotedAndRepeatsAreSkipped)
    {
      Agent::ProcessOutputStore store{ nullptr };
      store.Add(Output(5, 0, "a\n"));
      store.Add(Output(5, 0, "a\n"));
      store.Add(Output(5, 5000, "b\n"));

      auto lines = store.Lines(5, 0).Lines;
      Assert::AreEqual(size_t(3), lines.size(), L"the repeat adds nothing");
      Assert::IsTrue(lines[1].find("more in the output file") != string::npos);
      Assert::AreEqual(string("b"), lines[2]);
    }

    TEST_METHOD(OnlyTheLastLinesAreKept)
    {
      Agent::ProcessOutputStore store{ nullptr };
      string text;
      for (size_t index = 0; index < Agent::ProcessOutputStore::MaxLines + 5; index++) text += format("{}\n", index);
      store.Add(Output(5, 0, text));

      auto lines = store.Lines(5, 0);
      Assert::AreEqual(uint64_t(5), lines.Oldest);
      Assert::AreEqual(string("5"), lines.Lines.front());
      Assert::AreEqual(uint64_t(Agent::ProcessOutputStore::MaxLines + 5), store.Processes().front().LineCount);
    }

    TEST_METHOD(EndedProcessesStayAndANewOneWithTheSameIdIsNew)
    {
      Agent::ProcessOutputStore store{ nullptr };
      store.Add(Output(5, 0, "first\n"));
      store.Add(Output(5, 6, "", false));
      Assert::IsFalse(store.Processes().front().IsRunning);

      store.Add(Output(5, 0, "second\n"));
      auto processes = store.Processes();
      Assert::AreEqual(size_t(2), processes.size());
      Assert::IsTrue(processes[0].IsRunning, L"newest first");
      Assert::AreEqual(string("second"), store.Lines(5, 0).Lines.front());
    }

    TEST_METHOD(EarlierOutputFilesAreListedWithTheirParts)
    {
      auto folder = filesystem::temp_directory_path() / format("SecondShell.Tests.History.{}", GetCurrentProcessId());
      filesystem::create_directories(folder);
      auto write = [&](const char* name, string_view text) { ofstream{ folder / name, ios::binary } << text; };
      write("20261104-203238-21856-npm.log", "first\nsecond\n");
      write("20261104-203238-21856-npm.2.log", "third\nfourth\n");
      write("20261105-101500-777-server.log", "only\n");
      write("notes.txt.log", "not an output file");

      auto history = Agent::ProcessOutputStore::History(folder);
      Assert::AreEqual(size_t(2), history.size());
      Assert::AreEqual(string("server"), history[0].Program, L"newest first");
      Assert::AreEqual(uint32_t(21856), history[1].ProcessId);
      Assert::AreEqual(size_t(2), history[1].Parts.size(), L"the parts together");
      Assert::AreEqual(uint64_t(26), history[1].Size);

      auto lines = Agent::ProcessOutputStore::ReadTail(history[1]);
      Assert::IsTrue(lines == vector<string>{ "first", "second", "third", "fourth" }, L"across the parts");
      Assert::IsTrue(Agent::ProcessOutputStore::ReadTail(history[1], 9) == vector<string>{ "fourth" }, L"whole lines from the end");
      filesystem::remove_all(folder);
    }

    TEST_METHOD(TheOldestFilesGoBeyondTheLimit)
    {
      using namespace std::chrono_literals;
      auto now = filesystem::file_time_type::clock::now();
      vector<Agent::StoredFile> files = { { "new", 400, now }, { "old", 300, now - 2h }, { "older", 500, now - 3h } };

      Assert::IsTrue(Agent::CaptureStore::OverLimit(files, 1200).empty());
      Assert::IsTrue(Agent::CaptureStore::OverLimit(files, 1000) == vector<filesystem::path>{ "older" });
      Assert::IsTrue(Agent::CaptureStore::OverLimit(files, 400) == vector<filesystem::path>{ "older", "old" });
    }
  };
}
