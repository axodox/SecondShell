#include "pch.h"
#include "DriverProtocol.h"
#include "HostProtocol.h"
#include "JsonSnapshot.h"
#include "KeyNames.h"
#include "MouseActions.h"
#include "ProtocolErrors.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(ProtocolTests)
  {
    //Serializes to text and parses back, validating first, as the JSON-RPC peer does with params
    template<typename T>
    static T Roundtrip(const T& value)
    {
      auto text = json_serializer<T>::to_json(value)->to_string();
      string_view view = text;
      auto json = json_value::from_string(view);

      auto validation = T::json_description.validate(*json);
      if (!validation) Assert::Fail(to_wstring(validation.error()).c_str());

      T result{};
      auto parsing = T::json_description.from_json(result, json.get());
      if (!parsing) Assert::Fail(to_wstring(parsing.error()).c_str());
      return result;
    }

    template<typename T>
    static string ValidationError(string_view text)
    {
      auto json = json_value::from_string(text);
      auto validation = T::json_description.validate(*json);
      Assert::IsFalse(bool(validation), to_wstring(format("{} should be rejected", text)).c_str());
      return validation.error();
    }

  public:
    TEST_METHOD(ActionTravelsToTheDriver)
    {
      auto click = make_value<MouseClickAction>();
      click->Button = MouseButton::Right;
      click->X = 12;
      click->Y = 34;

      ExecuteActionRequest request;
      request.ActionId = 7;
      request.Action = click;

      auto received = Roundtrip(request);
      Assert::AreEqual(uint64_t(7), received.ActionId);
      Assert::IsTrue(received.Action->Type() == ActionType::MouseClick);

      auto& receivedClick = static_cast<const MouseClickAction&>(*received.Action);
      Assert::IsTrue(receivedClick.Button == MouseButton::Right);
      Assert::AreEqual(12, *receivedClick.X);
    }

    TEST_METHOD(ResultsRoundtripWithTheirType)
    {
      auto capture = make_value<CaptureResult>();
      capture->Tag = "after";
      capture->Image = { .Width = 640, .Height = 360, .Scale = 0.5, .Format = ImageFormat::Png, .Data = { 1, 2, 3 } };

      auto process = make_value<ProcessResult>();
      process->ProcessId = 42;
      process->ExitCode = -1;
      process->StandardOutput = "out";

      auto windows = make_value<WindowListResult>();
      windows->Windows.push_back({ .Window = { 0x1234 }, .Title = "Notepad", .ProcessId = 9, .State = WindowState::Maximized, .IsForeground = true });

      auto failed = make_value<ActionResult>();
      failed->Status = ActionStatus::Failed;
      failed->Error = ErrorCode::InvalidArgument;
      failed->Message = "No such window.";

      ExecuteActionResponse response;
      for (value_ptr<ActionResult> result : { value_ptr<ActionResult>(capture), value_ptr<ActionResult>(process), value_ptr<ActionResult>(windows), value_ptr<ActionResult>(failed) })
      {
        response.Result = result;
        auto received = Roundtrip(response);
        Assert::IsTrue(typeid(*received.Result) == typeid(*result), L"the result keeps its type");
        Assert::AreEqual(JsonSnapshot::ToCanonicalString(*json_serializer<value_ptr<ActionResult>>::to_json(result)), JsonSnapshot::ToCanonicalString(*json_serializer<value_ptr<ActionResult>>::to_json(received.Result)));
      }

      response.Result = capture;
      auto receivedCapture = static_cast<const CaptureResult&>(*Roundtrip(response).Result);
      Assert::AreEqual(size_t(3), receivedCapture.Image.Data.size(), L"image data travels as base64");
    }

    TEST_METHOD(SequenceStepsAreValidatedAsActions)
    {
      auto error = ValidationError<RunSequenceRequest>(R"({"steps": [{"action": "Mouse.Click"}, {"action": "Mouse.Scroll"}]})");
      Assert::IsTrue(error.find("'steps': Item 1: Unknown action 'Mouse.Scroll'") != string::npos, to_wstring(error).c_str());

      error = ValidationError<RunSequenceRequest>(R"({"steps": []})");
      Assert::IsTrue(error.find("at least 1") != string::npos, to_wstring(error).c_str());

      RunSequenceRequest request;
      string_view text = R"({"steps": [{"action": "Mouse.Click", "x": 1, "y": 2}, {"action": "Capture.Screen"}], "progressToken": "abc"})";
      auto json = json_value::from_string(text);
      Assert::IsTrue(bool(RunSequenceRequest::json_description.validate(*json)));
      Assert::IsTrue(bool(RunSequenceRequest::json_description.from_json(request, json.get())));
      Assert::AreEqual(size_t(2), request.Steps.size());
      Assert::IsTrue(request.Steps[1]->Type() == ActionType::CaptureScreen);
      Assert::AreEqual(string("abc"), request.ProgressToken->as<json_string>().value);
    }

    TEST_METHOD(UserInputIsForwardedAsActions)
    {
      auto move = make_value<MouseMoveAction>();
      move->X = 5;
      move->Y = 6;

      ForwardUserInputNotification notification;
      notification.Inputs.push_back(move);

      auto received = Roundtrip(notification);
      Assert::IsTrue(received.Inputs[0]->Type() == ActionType::MouseMove);
    }

    TEST_METHOD(SessionMessagesRoundtrip)
    {
      SessionResponse response;
      response.Session = { .State = SessionState::Ready, .SessionId = 12, .Display = DisplaySettings{ .Width = 2560, .Height = 1440, .Scale = 150 }, .IsDriverConnected = true, .DriverProcessId = 4321 };

      auto received = Roundtrip(response);
      Assert::IsTrue(received.Session.State == SessionState::Ready);
      Assert::AreEqual(2560u, received.Session.Display->Width);
      Assert::AreEqual(150u, received.Session.Display->Scale);

      auto error = ValidationError<SetDisplayRequest>(R"({"display": {"width": 100, "height": 1080, "scale": 100, "orientation": "Landscape"}})");
      Assert::IsTrue(error.find("'width'") != string::npos, to_wstring(error).c_str());
    }

    TEST_METHOD(DisplayChangesKeepWhatTheyLeaveOut)
    {
      DisplayChange change{ .Scale = 150 };
      auto display = change.ApplyTo({ .Width = 2560, .Height = 1440, .Scale = 100, .Orientation = DisplayOrientation::Portrait });
      Assert::AreEqual(2560u, display.Width);
      Assert::AreEqual(1440u, display.Height);
      Assert::AreEqual(150u, display.Scale);
      Assert::IsTrue(display.Orientation == DisplayOrientation::Portrait);
    }

    TEST_METHOD(ErrorCodesMapToRpcErrors)
    {
      for (auto& code : enum_values<ErrorCode>())
      {
        auto error = ProtocolErrors::Make(code.value, "message");
        Assert::IsTrue(ProtocolErrors::TryGetCode(error) == code.value);
      }

      Assert::IsFalse(ProtocolErrors::TryGetCode(JsonRpcError{ JsonRpcErrorCode::InternalError, "x" }).has_value());
    }

    TEST_METHOD(KeyNamesHaveCanonicalNames)
    {
      Assert::AreEqual(string("Enter"), KeyNames::ToString(0x0D));
      Assert::AreEqual(string("PageUp"), KeyNames::ToString(0x21));
      Assert::AreEqual(string("A"), KeyNames::ToString('A'));
      Assert::AreEqual(string("255"), KeyNames::ToString(0xFF), L"a code without a name is written as a number");

      Assert::IsTrue(KeyNames::TryParse("Return") == uint16_t(0x0D));
      Assert::IsTrue(KeyNames::TryParse(" numpad1 ") == uint16_t(0x61));
      Assert::IsFalse(KeyNames::TryParse("Hyper").has_value());
    }

    TEST_METHOD(WindowIdsAreHexadecimal)
    {
      Assert::AreEqual(string("0x0001A2B4"), WindowId{ 0x1A2B4 }.to_string());
      Assert::IsTrue(WindowId::from_string("0x0001A2B4") == WindowId{ 0x1A2B4 });
      Assert::IsTrue(WindowId::from_string("1a2b4") == WindowId{ 0x1A2B4 });
      Assert::IsFalse(WindowId::from_string("0x").has_value());
      Assert::IsFalse(WindowId::from_string("window").has_value());
    }
  };
}
