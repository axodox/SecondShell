#pragma once
#include "ActionLog.h"
#include "AgentControl.h"
#include "CaptureStore.h"
#include "IDriverLink.h"
#include "VisualizationFeed.h"

namespace SecondShell::Agent
{
  using ProgressCallback = std::function<void(uint32_t completed, uint32_t total, std::string_view message)>;

  //Runs one sequence at a time (Design.md, Errors and timeouts): the steps in order, Control.* itself and the rest on
  //the driver, async actions until Control.Join, the sequence's timeout, and the user's Stop. It stops at the first
  //failed step, releases what is held, and reports it.
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

  private:
    std::shared_ptr<Session::IDriverLink> _driver;
    Axodox::Infrastructure::event_subscription _actionCompletedSubscription;
    std::shared_ptr<AgentControl> _agentControl;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    std::shared_ptr<CaptureStore> _captureStore;
    std::shared_ptr<Session::SettingsStore> _settings;
    std::shared_ptr<ActionLog> _actionLog;
    std::shared_ptr<VisualizationFeed> _visualization;

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

    Protocol::RunSequenceResponse Failure(Actions::ErrorCode error, std::string message) const;
    std::string RefusalMessage() const;
    void ReportFailure(Protocol::RunSequenceResponse& response, const RunState& state, uint32_t index, const Actions::ActionResult& result) const;

    void OnActionCompleted(Session::IDriverLink* sender, const Protocol::ActionCompletedNotification& notification);
    void OnModeChanged(AgentControl* sender, ControlMode mode);
  };
}
