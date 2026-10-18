#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "ProcessActions.h"
#include "ProcessManager.h"

namespace SecondShell::Driver
{
  //Performs the Process.* actions.
  class ProcessActionHandler
  {
  public:
    explicit ProcessActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Desktop::ProcessManager> _processes;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Run(const Actions::Action& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> RunProcess(const Actions::ProcessRunAction& action, const Rpc::CancellationToken& cancellation);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Kill(const Actions::ProcessKillAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Enumerate();

    //The result of a wait: the process's result, or why there is none.
    static Axodox::Infrastructure::value_ptr<Actions::ActionResult> Exited(std::optional<Actions::ProcessResult>&& result, uint32_t processId, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation);
  };
}
