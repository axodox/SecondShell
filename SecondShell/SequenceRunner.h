#pragma once
#include "ActionLog.h"
#include "AgentControl.h"
#include "CaptureStore.h"
#include "IDriverLink.h"
#include "IInputAvailability.h"
#include "VisualizationFeed.h"

namespace SecondShell::Agent
{
  //Steps completed, of the total; while a long step runs, how far it is (0..1), now and then (KeepAliveInterval).
  using ProgressCallback = std::function<void(uint32_t completed, uint32_t total, std::optional<double> stepProgress, std::string_view message)>;

  //Runs one sequence at a time (Design.md, Errors and timeouts): the steps in order, Control.* itself and the rest on
  //the driver, async actions until Control.Join, the sequence's timeout, and the user's Stop. It stops at the first
  //failed step, releases what is held, and reports it. Input steps wait out the user's session switching to or from
  //the connection that keeps it unlocked behind the lock screen (Design.md, Working while the screen is locked).
  class SequenceRunner
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SequenceRunner" };
    Axodox::Infrastructure::event_owner _events;

    struct RunState;

  public:
    explicit SequenceRunner(Axodox::Infrastructure::dependency_container* container);

    Protocol::RunSequenceResponse Run(const Protocol::RunSequenceRequest& request, const ProgressCallback& progress, const Rpc::CancellationToken& cancellation);

    //A sequence started (true) or ended (false): the agent is using the session meanwhile.
    Axodox::Infrastructure::event_publisher<SequenceRunner*, bool> RunningChanged;

    //While a long step runs, progress is reported after this long without any: well within the minute after which AI
    //clients often give up on a tool call, for those which reset that timeout on progress. Tests shorten it.
    std::chrono::milliseconds KeepAliveInterval{ 20000 };

  private:
    std::shared_ptr<Session::IDriverLink> _driver;
    Axodox::Infrastructure::event_subscription _actionCompletedSubscription;
    std::shared_ptr<AgentControl> _agentControl;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    std::shared_ptr<CaptureStore> _captureStore;
    std::shared_ptr<Session::SettingsStore> _settings;
    std::shared_ptr<ActionLog> _actionLog;
    std::shared_ptr<VisualizationFeed> _visualization;
    std::shared_ptr<Session::IInputAvailability> _input;

    std::mutex _runMutex;  //held while a sequence runs
    std::atomic_uint64_t _nextActionId = 1;
    std::atomic_uint32_t _nextSequence = 1;

    //Guards _run and the async state in it
    std::mutex _mutex;
    std::condition_variable _asyncChanged;
    RunState* _run = nullptr;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> RunStep(RunState& state, const Axodox::Infrastructure::value_ptr<Actions::Action>& step, uint32_t index);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Wait(RunState& state, std::chrono::milliseconds duration);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Join(RunState& state);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Reset(RunState& state);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Capture(RunState& state, const Axodox::Infrastructure::value_ptr<Actions::Action>& step, uint32_t index);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> ReadClipboard(RunState& state, const Axodox::Infrastructure::value_ptr<Actions::Action>& step, uint32_t index);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> RunProcess(RunState& state, const Axodox::Infrastructure::value_ptr<Actions::Action>& step);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> StoreImage(RunState& state, Axodox::Infrastructure::value_ptr<Actions::ActionResult>&& result, Actions::CapturedImage& image, const std::optional<std::string>& path, const CaptureDestination& destination, std::string_view fileStem);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(RunState& state, const Axodox::Infrastructure::value_ptr<Actions::Action>& action);

    //An input step: after any switch of the user's session, and again, a few times, if it was refused during one.
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> ExecuteInput(RunState& state, const Axodox::Infrastructure::value_ptr<Actions::Action>& step);

    //Null once input is no longer resuming, or after the limit; a result if the sequence was stopped or timed out.
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> WaitWhileResuming(RunState& state);

    Protocol::RunSequenceResponse Failure(Actions::ErrorCode error, std::string message) const;
    std::string RefusalMessage() const;
    void ReportFailure(Protocol::RunSequenceResponse& response, const RunState& state, uint32_t index, const Actions::ActionResult& result) const;

    void OnActionCompleted(Session::IDriverLink* sender, const Protocol::ActionCompletedNotification& notification);
    void OnModeChanged(AgentControl* sender, ControlMode mode);
  };
}
