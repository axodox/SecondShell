#include "pch.h"
#include "CaptureActions.h"
#include "ProtocolErrors.h"
#include "SequenceRunner.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  namespace
  {
    //A driver which records the actions it gets, and answers them with Respond.
    class FakeDriverLink : public IDriverLink
    {
    public:
      bool Connected = true;
      InputState Held;
      function<value_ptr<ActionResult>(const Action& action)> Respond = [](const Action&) { return ActionResult::Succeeded(); };

      vector<ExecuteActionRequest> Requests() const
      {
        lock_guard lock{ _mutex };
        return _requests;
      }

      int ResetCount() const
      {
        lock_guard lock{ _mutex };
        return _resetCount;
      }

      void Complete(uint64_t actionId, value_ptr<ActionResult> result)
      {
        OnActionCompleted(ActionCompletedNotification{ actionId, move(result) });
      }

      virtual bool IsConnected() const override { return Connected; }
      virtual optional<uint32_t> DriverProcessId() const override { return 1; }

      virtual RpcResult<ExecuteActionResponse> Execute(const ExecuteActionRequest& request, milliseconds /*timeout*/, const CancellationToken& /*cancellation*/) override
      {
        {
          lock_guard lock{ _mutex };
          _requests.push_back(request);
        }
        return ExecuteActionResponse{ Respond(*request.Action) };
      }

      virtual RpcResult<ResetInputResponse> ResetInput() override
      {
        lock_guard lock{ _mutex };
        _resetCount++;
        return ResetInputResponse{ exchange(Held, {}) };
      }

    private:
      mutable mutex _mutex;
      vector<ExecuteActionRequest> _requests;
      int _resetCount = 0;
    };
  }

  TEST_CLASS(SequenceRunnerTests)
  {
    dependency_container _container;
    shared_ptr<FakeDriverLink> _driver = make_shared<FakeDriverLink>();
    CancellationToken _noCancellation;

    static RunSequenceRequest Request(string_view text)
    {
      auto json = json_value::from_string(text);
      auto validation = RunSequenceRequest::json_description.validate(*json);
      if (!validation) Assert::Fail(to_wstring(validation.error()).c_str());

      RunSequenceRequest request;
      RunSequenceRequest::json_description.from_json(request, json.get()).check();
      return request;
    }

    RunSequenceResponse Run(string_view text)
    {
      return _container.resolve<SequenceRunner>()->Run(Request(text), nullptr, _noCancellation);
    }

  public:
    SequenceRunnerTests()
    {
      _container.add<IDriverLink>(_driver);
    }

    TEST_METHOD(StepsRunInOrderWithProgress)
    {
      vector<uint32_t> progress;
      auto response = _container.resolve<SequenceRunner>()->Run(Request(R"({"steps": [{"action": "Mouse.Click"}, {"action": "Control.Wait", "duration": 10}, {"action": "Keyboard.Type", "text": "a"}]})"),
        [&](uint32_t completed, uint32_t total, string_view) { progress.push_back(completed); Assert::AreEqual(3u, total); }, _noCancellation);

      Assert::IsFalse(response.FailedStep.has_value());
      Assert::AreEqual(size_t(3), response.Results.size(), L"one result per step");
      Assert::IsTrue(progress == vector<uint32_t>{ 1, 2, 3 });

      auto requests = _driver->Requests();
      Assert::AreEqual(size_t(2), requests.size(), L"Control.Wait runs in the host");
      Assert::IsTrue(requests[0].Action->Type() == ActionType::MouseClick && requests[1].Action->Type() == ActionType::KeyboardType);
    }

    TEST_METHOD(AFailedStepStopsTheSequenceAndReleasesInput)
    {
      _driver->Held.MouseButtons = { MouseButton::Left };
      _driver->Respond = [](const Action& action) {
        return action.Type() == ActionType::KeyboardType ? ActionResult::Failed(ErrorCode::InvalidArgument, "Bad text.") : ActionResult::Succeeded();
        };

      auto response = Run(R"({"steps": [{"action": "Mouse.Press"}, {"action": "Keyboard.Type", "text": "a"}, {"action": "Mouse.Release"}]})");
      Assert::AreEqual(1u, *response.FailedStep);
      Assert::IsTrue(response.Error == ErrorCode::InvalidArgument);
      Assert::AreEqual(string("Bad text."), *response.Message);
      Assert::AreEqual(size_t(1), response.Results.size(), L"the results of the completed steps");
      Assert::AreEqual(size_t(2), _driver->Requests().size(), L"the step after the failure does not run");
      Assert::IsTrue(response.ReleasedInput->MouseButtons == vector<MouseButton>{ MouseButton::Left }, L"what was held is reported");
    }

    TEST_METHOD(JoinWaitsForAsyncActionsAndReportsTheirFailure)
    {
      //The driver answers async actions at once, and reports them later
      thread completer;
      _driver->Respond = [&](const Action&) {
        auto actionId = _driver->Requests().back().ActionId;
        completer = thread([this, actionId] {
          this_thread::sleep_for(50ms);
          _driver->Complete(actionId, ActionResult::Failed(ErrorCode::ActionFailed, "The move failed."));
          });
        return ActionResult::Succeeded();
        };

      auto start = steady_clock::now();
      auto response = Run(R"({"steps": [{"action": "Mouse.Move", "x": 1, "y": 1, "duration": 50, "async": true}, {"action": "Control.Join"}]})");
      completer.join();

      Assert::IsTrue(steady_clock::now() - start >= 40ms, L"the join waits for the action");
      Assert::AreEqual(1u, *response.FailedStep, L"the join fails with the async action");
      Assert::AreEqual(string("The move failed."), *response.Message);
    }

    TEST_METHOD(TheSequenceTimeoutStopsIt)
    {
      auto start = steady_clock::now();
      auto response = Run(R"({"steps": [{"action": "Control.Wait", "duration": 5000}], "timeout": 100})");

      Assert::IsTrue(steady_clock::now() - start < 2s);
      Assert::AreEqual(0u, *response.FailedStep);
      Assert::IsTrue(response.Error == ErrorCode::Timeout);
    }

    TEST_METHOD(StopInterruptsAndLocksOutTheAgent)
    {
      auto agentControl = _container.resolve<AgentControl>();
      thread stopper([&] {
        this_thread::sleep_for(50ms);
        agentControl->Stop();
        });

      auto response = Run(R"({"steps": [{"action": "Control.Wait", "duration": 5000}]})");
      stopper.join();
      Assert::IsTrue(response.Error == ErrorCode::UserInterrupted);
      Assert::AreEqual(1, _driver->ResetCount(), L"input is released");

      response = Run(R"({"steps": [{"action": "Mouse.Click"}]})");
      Assert::IsTrue(response.Error == ErrorCode::AgentInputDisabled);
      Assert::IsFalse(response.FailedStep.has_value());

      agentControl->GiveBackControl();
      Assert::IsFalse(Run(R"({"steps": [{"action": "Mouse.Click"}]})").Error.has_value());
    }

    TEST_METHOD(WithoutDriverThereIsNoSession)
    {
      _driver->Connected = false;
      auto response = Run(R"({"steps": [{"action": "Mouse.Click"}]})");
      Assert::IsTrue(response.Error == ErrorCode::SessionNotActive);
      Assert::IsTrue(_driver->Requests().empty());
    }

    TEST_METHOD(CapturesBeyondTheInlineLimitGoToFile)
    {
      _driver->Respond = [](const Action& action) -> value_ptr<ActionResult> {
        auto& capture = static_cast<const CaptureAction&>(action);
        auto result = make_value<CaptureResult>();
        result->Tag = *capture.Tag;
        result->Image = { .Width = 2, .Height = 1, .Format = *capture.Format, .Data = { 1, 2, 3 } };
        return result;
        };

      _container.resolve<CaptureStore>()->Configure({ .InlineLimit = 1 });
      auto folder = filesystem::temp_directory_path() / format("SecondShell.Tests.{}", GetCurrentProcessId());
      auto file = folder / "second.png";
      auto steps = format(R"({{"steps": [{{"action": "Capture.Screen"}}, {{"action": "Capture.Screen", "path": "{}"}}]}})", json_escape_string(winrt::to_string(file.wstring())));
      auto response = Run(steps);

      auto& first = static_cast<const CaptureResult&>(*response.Results[0]);
      Assert::AreEqual(string("0"), first.Tag, L"the tag defaults to the step's index");
      Assert::AreEqual(size_t(3), first.Image.Data.size(), L"the first capture is inline");

      auto& second = static_cast<const CaptureResult&>(*response.Results[1]);
      Assert::IsTrue(second.Image.Data.empty(), L"the second is not inline");
      Assert::AreEqual(winrt::to_string(file.wstring()), *second.Image.Path);
      Assert::IsTrue(second.Message.has_value(), L"the result says why");
      Assert::AreEqual(uintmax_t(3), filesystem::file_size(file));

      filesystem::remove_all(folder);
    }
  };
}
