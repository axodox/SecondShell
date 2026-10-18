#include "pch.h"
#include "AppEnvironment.h"
#include "CaptureEncoder.h"
#include "ChildSessionApi.h"
#include "HostWindow.h"
#include "McpProbe.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Capture;
using namespace SecondShell::Environment;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono_literals;
using namespace winrt::Windows::Globalization;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Storage::Streams;

namespace SecondShell::Tests
{
  namespace
  {
    const json_object& Structured(const value_ptr<json_value>& result)
    {
      return result->as<json_object>().at("structuredContent")->as<json_object>();
    }

    const json_object& SessionOf(const value_ptr<json_value>& result)
    {
      return Structured(result).at("session")->as<json_object>();
    }

    string StateOf(const value_ptr<json_value>& result)
    {
      return SessionOf(result).at("state")->as<json_string>().value;
    }

    double DriverProcessIdOf(const value_ptr<json_value>& result)
    {
      return SessionOf(result).at("driverPid")->as<json_number>().value;
    }

    bool IsError(const value_ptr<json_value>& result)
    {
      auto& object = result->as<json_object>();
      return object.value.contains("isError") && object.at("isError")->as<json_boolean>().value;
    }

    const json_object& ResultAt(const value_ptr<json_value>& result, size_t index)
    {
      return Structured(result).at("results")->as<json_array>().value.at(index)->as<json_object>();
    }

    string StringOf(const json_object& object, const char* name)
    {
      return object.at(name)->as<json_string>().value;
    }

    double NumberOf(const json_object& object, const char* name)
    {
      return object.at(name)->as<json_number>().value;
    }

    //The text with each %name% replaced by its value
    string With(string text, initializer_list<pair<string_view, string>> values)
    {
      for (auto& [name, value] : values)
      {
        auto placeholder = format("%{}%", name);
        for (auto position = text.find(placeholder); position != string::npos; position = text.find(placeholder, position + value.size()))
        {
          text.replace(position, placeholder.size(), value);
        }
      }
      return text;
    }

    vector<uint8_t> FirstImageOf(const value_ptr<json_value>& result)
    {
      for (auto& block : result->as<json_object>().at("content")->as<json_array>().value)
      {
        auto& content = block->as<json_object>();
        if (content.at("type")->as<json_string>().value != "image") continue;

        vector<uint8_t> image;
        Assert::IsTrue(try_decode_base64(content.at("data")->as<json_string>().value, image));
        return image;
      }

      Assert::Fail(L"the result should contain an image");
    }

    //Runs on a thread of the MTA, as the tests run on an STA thread, which must not block on async calls.
    string RecognizeText(const vector<uint8_t>& image)
    {
      auto engine = OcrEngine::TryCreateFromLanguage(Language{ L"en-US" });
      Assert::IsTrue(bool(engine), L"the English OCR language should be installed");

      return async(launch::async, [&] {
        winrt::init_apartment();
        InMemoryRandomAccessStream stream;
        DataWriter writer{ stream };
        writer.WriteBytes(image);
        writer.StoreAsync().get();
        writer.DetachStream();
        stream.Seek(0);

        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        auto bitmap = decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied).get();
        return winrt::to_string(engine.RecognizeAsync(bitmap).get().Text());
        }).get();
    }

    //Starts the driver in this session, as a leftover startup entry would at a normal logon, and returns its exit code.
    DWORD RunDriverInThisSession()
    {
      STARTUPINFOW startup{ .cb = sizeof(STARTUPINFOW) };
      PROCESS_INFORMATION process{};
      auto commandLine = format(L"\"{}\"", AppEnvironment().DriverExecutable().wstring());
      winrt::check_bool(CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process));
      winrt::handle driver{ process.hProcess };
      CloseHandle(process.hThread);

      Assert::AreEqual(DWORD(WAIT_OBJECT_0), WaitForSingleObject(driver.get(), 15000), L"the driver should exit");
      DWORD exitCode = 0;
      GetExitCodeProcess(driver.get(), &exitCode);
      return exitCode;
    }

    //Closes the host's window, as the user would, so the next tests find no host running.
    void CloseHost()
    {
      auto snapshot = winrt::handle{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
      PROCESSENTRY32W entry{ .dwSize = sizeof(PROCESSENTRY32W) };
      for (auto found = Process32FirstW(snapshot.get(), &entry); found; found = Process32NextW(snapshot.get(), &entry))
      {
        if (_wcsicmp(entry.szExeFile, AppEnvironment().HostExecutable().filename().c_str()) != 0) continue;

        winrt::handle host{ OpenProcess(SYNCHRONIZE, FALSE, entry.th32ProcessID) };
        if (!host) continue;

        EnumWindows([](HWND window, LPARAM processId) {
          DWORD windowProcessId = 0;
          GetWindowThreadProcessId(window, &windowProcessId);
          if (windowProcessId == DWORD(processId) && IsWindowVisible(window)) PostMessageW(window, WM_CLOSE, 0, 0);
          return TRUE;
          }, LPARAM(entry.th32ProcessID));

        Assert::AreEqual(DWORD(WAIT_OBJECT_0), WaitForSingleObject(host.get(), 30000), L"the host should exit when its window closes");
      }
    }
  }

  //Drives the child session through the real MCP server, host and driver, one session for the class. It needs the
  //password saved by a first manual Initialize-Session, logs off any child session that exists, and expects that no
  //SecondShell host is running; so it is not part of the normal test run (build.ps1 -SessionTests runs it). The tests
  //type only into consoles, which restore nothing from the user's profile, which the child session shares.
  TEST_CLASS(SessionProbeTests)
  {
    inline static unique_ptr<McpProbe> _probe;

    //Runs steps written over several lines; stdio frames messages by line, so they are sent as one.
    static value_ptr<json_value> Run(string steps, chrono::milliseconds timeout = 60s)
    {
      ranges::replace(steps, '\n', ' ');
      return _probe->CallTool("Run-Sequence", format(R"({{"steps": {}}})", steps), timeout);
    }

    //A failed step is not a tool error: the result says which step failed, and why
    static value_ptr<json_value> RunSucceeding(string steps, chrono::milliseconds timeout = 60s)
    {
      auto result = Run(move(steps), timeout);
      Assert::IsFalse(IsError(result) || Structured(result).value.contains("error"), to_wstring(result->to_string().substr(0, 2000)).c_str());
      return result;
    }

    //The error's code; the message is checked by the caller.
    static string RunFailing(string steps, string& message)
    {
      auto result = Run(move(steps));
      auto& structured = Structured(result);
      Assert::IsTrue(structured.value.contains("error"), L"the sequence should fail");
      message = StringOf(structured, "message");
      return StringOf(structured, "error");
    }

    static set<string> WindowIds()
    {
      set<string> windows;
      auto result = RunSucceeding(R"([{ "action": "Window.Enumerate" }])");
      for (auto& window : ResultAt(result, 0).at("windows")->as<json_array>().value) windows.insert(StringOf(window->as<json_object>(), "window"));
      return windows;
    }

    //A console of its own, started as the agent would; its window reports the cmd in it as its process. It is awaited
    //by a text inside its banner: OCR misses the letter at the window's very edge.
    static pair<string, uint32_t> StartConsole()
    {
      auto before = WindowIds();
      RunSucceeding(R"([
        { "action": "Process.Run", "command": "conhost.exe", "shell": "None", "args": ["cmd.exe", "/d"], "async": true },
        { "action": "Capture.WaitForText", "text": "All rights reserved" }
      ])");

      auto result = RunSucceeding(R"([{ "action": "Window.Enumerate" }])");
      for (auto& item : ResultAt(result, 0).at("windows")->as<json_array>().value)
      {
        auto& window = item->as<json_object>();
        if (!before.contains(StringOf(window, "window"))) return { StringOf(window, "window"), uint32_t(NumberOf(window, "pid")) };
      }

      Assert::Fail(L"the console should have a window");
    }

    //That the session's screen keeps changing: a console opened by the agent shows in the next capture. Without OCR,
    //which depends on the wallpaper.
    static void AssertTheScreenFollows(const wchar_t* when)
    {
      auto before = FirstImageOf(RunSucceeding(R"([{ "action": "Capture.Screen" }])"));
      auto opened = RunSucceeding(R"([
        { "action": "Process.Run", "command": "conhost.exe", "shell": "None", "args": ["cmd.exe", "/d"], "async": true },
        { "action": "Control.Wait", "duration": 1500 },
        { "action": "Capture.Screen" }
      ])");
      Assert::IsTrue(FirstImageOf(opened) != before, format(L"the capture should show the new console {}", when).c_str());

      auto pid = to_string(uint32_t(NumberOf(ResultAt(opened, 0), "pid")));
      RunSucceeding(With(R"([{ "action": "Process.Kill", "pid": %pid%, "tree": true }])", { { "pid", pid } }));
    }

  public:
    TEST_CLASS_INITIALIZE(StartSession)
    {
      _probe = make_unique<McpProbe>();
      _probe->Initialize();

      //Signs in with the saved password, and accepts the driver started by RunOnce
      auto session = _probe->CallTool("Initialize-Session", R"({"existingSession": "Reset"})", 120s);
      Assert::IsFalse(IsError(session), to_wstring(session->to_string()).c_str());
      Assert::AreEqual(string("Ready"), StateOf(session));
    }

    //Also after a failure: logs the session off, and closes the host
    TEST_CLASS_CLEANUP(StopSession)
    {
      if (_probe)
      {
        _probe->CallTool("Shutdown-Session", "{}", 60s);
        auto state = StateOf(_probe->CallTool("Check-Session", "{}", 10s));
        _probe->Close();
        _probe.reset();

        Assert::AreEqual(string("Stopped"), state);
        Assert::IsFalse(ChildSessionApi().ChildSessionId().has_value(), L"the child session should be gone");
      }

      CloseHost();
    }

    TEST_METHOD(TheSessionKeepsWorkingWhateverTheWindowDoes)
    {
      HostWindow host;
      WINDOWPLACEMENT placement{ .length = sizeof(WINDOWPLACEMENT) };
      GetWindowPlacement(host.Handle(), &placement);

      ShowWindow(host.Handle(), SW_MINIMIZE);
      AssertTheScreenFollows(L"while the window is minimized");

      ShowWindow(host.Handle(), SW_RESTORE);
      SetWindowPos(host.Handle(), nullptr, 0, 0, 900, 600, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      AssertTheScreenFollows(L"after the window was resized");

      SetWindowPos(host.Handle(), nullptr, 60, 60, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      AssertTheScreenFollows(L"after the window was moved");

      SetWindowPlacement(host.Handle(), &placement);
    }

    TEST_METHOD(StopInterruptsTheAgentUntilControlIsGivenBack)
    {
      auto running = async(launch::async, [] { return Run(R"([{ "action": "Control.Wait", "duration": 20000 }])"); });
      this_thread::sleep_for(1s);

      HostWindow host;
      host.Click(L"Stop");
      auto result = running.get();
      auto& interrupted = Structured(result);
      Assert::AreEqual(string("UserInterrupted"), StringOf(interrupted, "error"));

      string message;
      Assert::AreEqual(string("AgentInputDisabled"), RunFailing(R"([{ "action": "Mouse.Move", "x": 10, "y": 10 }])", message), L"stopped, the agent is refused");

      host.Click(L"Give back control");
      RunSucceeding(R"([{ "action": "Mouse.Move", "x": 10, "y": 10 }])");
    }

    TEST_METHOD(TypedTextAppearsInTheCapture)
    {
      auto result = RunSucceeding(R"([
        { "action": "Keyboard.Tap", "key": "R", "modifiers": ["Win"] },
        { "action": "Capture.WaitForStable" },
        { "action": "Keyboard.Type", "text": "conhost.exe cmd.exe /d\n" },
        { "action": "Capture.WaitForText", "text": "All rights reserved" },
        { "action": "Keyboard.Type", "text": "echo Typed into the second session\n" },
        { "action": "Capture.WaitForText", "text": "Typed into the second session" },
        { "action": "Capture.Screen" }
      ])");

      //The capture, not only the driver's own OCR, shows it; the text has no "l", which OCR reads as "1" in consoles
      auto text = RecognizeText(FirstImageOf(result));
      Assert::IsTrue(text.find("Typed into the second session") != string::npos, to_wstring(text).c_str());
    }

    TEST_METHOD(TheDriverRefusesTheUserSession)
    {
      auto driver = DriverProcessIdOf(_probe->CallTool("Check-Session", "{}", 10s));

      //A driver started in the user's session exits at once, and the child session keeps its own driver
      Assert::AreEqual(DWORD(1), RunDriverInThisSession());
      auto check = _probe->CallTool("Check-Session", "{}", 10s);
      Assert::AreEqual(string("Ready"), StateOf(check));
      Assert::AreEqual(driver, DriverProcessIdOf(check));
    }

    TEST_METHOD(CommandsReportTheirOutput)
    {
      auto result = RunSucceeding(R"([
        { "action": "Process.Run", "command": "Write-Output 'out ő'; [Console]::Error.WriteLine('err'); exit 3" },
        { "action": "Process.Run", "command": "echo %CD%& exit 7", "shell": "Cmd", "workingDirectory": "C:\\Windows" },
        { "action": "Process.Run", "command": "cmd.exe", "shell": "None", "args": ["/d", "/c", "echo", "a&b"] }
      ])");

      auto& powerShell = ResultAt(result, 0);
      Assert::AreEqual(3.0, NumberOf(powerShell, "exitCode"));
      Assert::IsTrue(StringOf(powerShell, "stdout").starts_with("out ő"), L"the output should be UTF-8");
      Assert::IsTrue(StringOf(powerShell, "stderr").starts_with("err"), L"errors should be text");

      auto& cmd = ResultAt(result, 1);
      Assert::AreEqual(7.0, NumberOf(cmd, "exitCode"));
      Assert::IsTrue(StringOf(cmd, "stdout").starts_with("C:\\Windows"), L"the command runs in its working directory");

      Assert::IsTrue(StringOf(ResultAt(result, 2), "stdout").starts_with("\"a&b\""), L"without a shell, & is an argument, not an operator");
    }

    TEST_METHOD(CommandsTimeOutWithTheirProcessId)
    {
      string message;
      Assert::AreEqual(string("Timeout"), RunFailing(R"([{ "action": "Process.Run", "command": "Start-Sleep 5", "timeout": 300 }])", message));
      Assert::IsTrue(message.find("keeps running") != string::npos, to_wstring(message).c_str());
    }

    TEST_METHOD(WindowsCanBeArranged)
    {
      auto window = StartConsole().first;
      auto result = RunSucceeding(With(R"([
        { "action": "Window.Focus", "window": "%window%" },
        { "action": "Window.Move", "window": "%window%", "x": 100, "y": 80 },
        { "action": "Window.Resize", "window": "%window%", "width": 800, "height": 500 },
        { "action": "Capture.Window", "window": "%window%" },
        { "action": "Window.Maximize", "window": "%window%" },
        { "action": "Window.Restore", "window": "%window%" },
        { "action": "Window.Minimize", "window": "%window%" },
        { "action": "Window.Restore", "window": "%window%" },
        { "action": "Window.Enumerate" }
      ])", { { "window", window } }));

      auto& capture = ResultAt(result, 3).at("image")->as<json_object>();
      Assert::AreEqual(800.0, NumberOf(capture, "width"), L"the capture shows the window's visible frame");
      Assert::AreEqual(500.0, NumberOf(capture, "height"));

      for (auto& item : ResultAt(result, 8).at("windows")->as<json_array>().value)
      {
        auto& info = item->as<json_object>();
        if (StringOf(info, "window") != window) continue;

        Assert::AreEqual(100.0, NumberOf(info, "x"));
        Assert::AreEqual(80.0, NumberOf(info, "y"));
        Assert::AreEqual(string("Normal"), StringOf(info, "state"));
        Assert::IsTrue(info.at("isForeground")->as<json_boolean>().value, L"a restored window gets the foreground back");
      }

      //Closed as with its X button; the console then ends cmd, and closes the window a moment later
      RunSucceeding(With(R"([{ "action": "Window.Close", "window": "%window%" }])", { { "window", window } }));
      auto isGone = false;
      for (auto attempt = 0; attempt < 20 && !isGone; attempt++)
      {
        isGone = !WindowIds().contains(window);
        if (!isGone) this_thread::sleep_for(100ms);
      }
      Assert::IsTrue(isGone, L"the window should be gone");
    }

    TEST_METHOD(ProcessTreesCanBeKilled)
    {
      auto result = RunSucceeding(R"([
        { "action": "Process.Run", "command": "cmd.exe", "shell": "None", "args": ["/d", "/c", "ping -n 60 127.0.0.1"], "async": true },
        { "action": "Control.Wait", "duration": 1000 }
      ])");
      auto root = to_string(uint32_t(NumberOf(ResultAt(result, 0), "pid")));

      auto pings = [&] {
        size_t count = 0;
        auto processes = RunSucceeding(R"([{ "action": "Process.Enumerate" }])");
        for (auto& item : ResultAt(processes, 0).at("processes")->as<json_array>().value)
        {
          auto& process = item->as<json_object>();
          if (to_string(uint32_t(NumberOf(process, "parentPid"))) == root && _stricmp(StringOf(process, "name").c_str(), "ping.exe") == 0) count++;
        }
        return count;
        };

      Assert::AreEqual(size_t(1), pings(), L"cmd should have started ping");
      RunSucceeding(With(R"([{ "action": "Process.Kill", "pid": %pid%, "tree": true }])", { { "pid", root } }));
      Assert::AreEqual(size_t(0), pings(), L"the tree should be gone");
    }

    TEST_METHOD(TheClipboardRoundTrips)
    {
      Pixels pixels{ 2, 1, { 255, 0, 0, 255, 0, 0, 255, 255 } };
      auto png = encode_base64(CaptureEncoder().Encode(pixels, nullopt, ImageFormat::Png).Data);

      auto result = RunSucceeding(With(R"([
        { "action": "Clipboard.Write", "content": [{ "format": "Text", "data": "Árvíztűrő" }, { "format": "Html", "data": "<b>Árvíztűrő</b>" }] },
        { "action": "Clipboard.Read" },
        { "action": "Clipboard.Read", "format": "Html" },
        { "action": "Clipboard.Write", "content": [{ "format": "Files", "data": ["C:\\Windows\\win.ini"] }] },
        { "action": "Clipboard.Read", "format": "Files" },
        { "action": "Clipboard.Write", "content": [{ "format": "Raw", "name": "SecondShell.Test", "data": "aGVsbG8=" }] },
        { "action": "Clipboard.Read", "format": "Raw", "name": "SecondShell.Test" },
        { "action": "Clipboard.Write", "content": [{ "format": "Image", "data": "%png%" }] },
        { "action": "Clipboard.Read", "format": "Image" }
      ])", { { "png", png } }));

      Assert::AreEqual(string("Árvíztűrő"), StringOf(ResultAt(result, 1).at("content")->as<json_object>(), "data"));
      Assert::AreEqual(string("<b>Árvíztűrő</b>"), StringOf(ResultAt(result, 2).at("content")->as<json_object>(), "data"));
      Assert::AreEqual(string("C:\\Windows\\win.ini"), ResultAt(result, 4).at("content")->as<json_object>().at("data")->as<json_array>().value.at(0)->as<json_string>().value);
      Assert::AreEqual(string("aGVsbG8="), StringOf(ResultAt(result, 6).at("content")->as<json_object>(), "data"));
      Assert::AreEqual(2.0, NumberOf(ResultAt(result, 8).at("image")->as<json_object>(), "width"));

      string message;
      Assert::AreEqual(string("ActionFailed"), RunFailing(R"([{ "action": "Clipboard.Read", "format": "Html" }])", message));
      Assert::IsTrue(message.find("PNG") != string::npos, L"the error lists what the clipboard has");
    }

    TEST_METHOD(ApplicationsPasteTheClipboard)
    {
      auto [window, client] = StartConsole();
      RunSucceeding(With(R"([
        { "action": "Clipboard.Write", "content": [{ "format": "Text", "data": "echo pasted into the session" }] },
        { "action": "Window.Focus", "window": "%window%" },
        { "action": "Keyboard.Tap", "key": "V", "modifiers": ["Ctrl"] },
        { "action": "Keyboard.Tap", "key": "Enter" },
        { "action": "Capture.WaitForText", "text": "pasted into the session" },
        { "action": "Process.Stop", "pid": %pid% }
      ])", { { "window", window }, { "pid", to_string(client) } }));
    }

    TEST_METHOD(WaitsTimeOutWithWhatTheyFound)
    {
      string message;
      Assert::AreEqual(string("Timeout"), RunFailing(R"([{ "action": "Capture.WaitForText", "text": "this never appears", "timeout": 1000 }])", message));
      Assert::IsTrue(message.find("The screen showed") != string::npos, to_wstring(message).c_str());

      //A corner of the wallpaper does not change
      RunSucceeding(R"([{ "action": "Capture.WaitForStable", "x": 1700, "y": 40, "width": 150, "height": 150 }])");
    }

    TEST_METHOD(DisplayChangesReachCaptures)
    {
      for (auto [width, height] : { pair{ 1280, 720 }, pair{ 1920, 1080 } })
      {
        auto display = _probe->CallTool("Set-Display", format(R"({{"display": {{"width": {}, "height": {}}}}})", width, height), 30s);
        Assert::IsFalse(IsError(display), to_wstring(display->to_string()).c_str());

        auto result = RunSucceeding(R"([{ "action": "Capture.Screen", "output": "File" }])");
        auto& image = ResultAt(result, 0).at("image")->as<json_object>();
        Assert::AreEqual(double(width), NumberOf(image, "width"));
        Assert::AreEqual(double(height), NumberOf(image, "height"));
      }

      //Only the scaling: the resolution stays
      for (auto scale : { 125, 100 })
      {
        auto display = _probe->CallTool("Set-Display", format(R"({{"display": {{"scale": {}}}}})", scale), 30s);
        Assert::IsFalse(IsError(display), to_wstring(display->to_string()).c_str());

        auto& settings = Structured(display).at("display")->as<json_object>();
        Assert::AreEqual(double(scale), NumberOf(settings, "scale"));
        Assert::AreEqual(1920.0, NumberOf(settings, "width"), L"what the change leaves out stays as it was");
        Assert::AreEqual(1080.0, NumberOf(settings, "height"));
      }
    }
  };
}
