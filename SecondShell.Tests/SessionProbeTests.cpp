#include "pch.h"
#include "AppEnvironment.h"
#include "ChildSessionApi.h"
#include "McpProbe.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
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
    const json_object& SessionOf(const value_ptr<json_value>& result)
    {
      return result->as<json_object>().at("structuredContent")->as<json_object>().at("session")->as<json_object>();
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
      return {};
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

    //Kept for a look when the test fails.
    filesystem::path Save(const vector<uint8_t>& image)
    {
      auto path = filesystem::temp_directory_path() / "SecondShell.SessionProbe.png";
      ofstream(path, ios::binary).write(reinterpret_cast<const char*>(image.data()), image.size());
      return path;
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

  //Signs in to the child session and drives it through the real MCP server, host and driver. It needs the password
  //saved by a first manual Initialize-Session, logs off any child session that exists, and expects that no SecondShell
  //host is running; so it is not part of the normal test run (build.ps1 -SessionTests runs it).
  TEST_CLASS(SessionProbeTests)
  {
  public:
    TEST_METHOD(DrivesTheChildSession)
    {
      McpProbe probe;
      probe.Initialize();

      //Signs in with the saved password, and accepts the driver started by RunOnce
      auto session = probe.CallTool("Initialize-Session", R"({"existingSession": "Reset"})", 120s);
      Assert::IsFalse(IsError(session), to_wstring(session->to_string()).c_str());
      Assert::AreEqual(string("Ready"), StateOf(session));
      auto driverProcessId = DriverProcessIdOf(session);

      //A console, unlike Notepad, restores nothing from the user's profile, which the child session shares; the
      //messages are single lines, as stdio frames them by line
      auto sequence = probe.CallTool("Run-Sequence", R"({"steps": [)"
        R"({ "action": "Keyboard.Tap", "key": "R", "modifiers": ["Win"] },)"
        R"({ "action": "Control.Wait", "duration": 1500 },)"
        R"({ "action": "Keyboard.Type", "text": "conhost.exe cmd.exe /d\n" },)"
        R"({ "action": "Control.Wait", "duration": 3000 },)"
        R"({ "action": "Keyboard.Type", "text": "echo Typed into the second session\n" },)"
        R"({ "action": "Control.Wait", "duration": 1000 },)"
        R"({ "action": "Capture.Screen" })"
        R"(]})", 60s);
      Assert::IsFalse(IsError(sequence), to_wstring(sequence->to_string()).c_str());

      auto image = FirstImageOf(sequence);
      auto text = RecognizeText(image);
      Logger::WriteMessage(format("Capture: {}\nRecognized: {}", Save(image).string(), text).c_str());
      //The text has no "l", which OCR reads as "1" in the console's font
      Assert::IsTrue(text.find("Typed into the second session") != string::npos, L"the capture should show the typed text");

      //Guard: a driver started in the user's session exits at once, and the child session keeps its own driver
      Assert::AreEqual(DWORD(1), RunDriverInThisSession());
      auto check = probe.CallTool("Check-Session", "{}", 10s);
      Assert::AreEqual(string("Ready"), StateOf(check));
      Assert::AreEqual(driverProcessId, DriverProcessIdOf(check));

      //Logs the session off
      probe.CallTool("Shutdown-Session", "{}", 60s);
      Assert::AreEqual(string("Stopped"), StateOf(probe.CallTool("Check-Session", "{}", 10s)));
      Assert::IsFalse(ChildSessionApi().ChildSessionId().has_value(), L"the child session should be gone");
    }

    //Also after a failure: logs the session off, and closes the host
    TEST_METHOD_CLEANUP(StopSession)
    {
      {
        McpProbe probe;
        probe.Initialize();
        probe.CallTool("Shutdown-Session", "{}", 60s);
      }

      CloseHost();
    }
  };
}
