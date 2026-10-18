#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "WindowActions.h"
#include "WindowManager.h"

namespace SecondShell::Driver
{
  //Performs the Window.* actions.
  class WindowActionHandler
  {
  public:
    explicit WindowActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Desktop::WindowManager> _windows;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Run(const Actions::Action& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Enumerate();
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Focus(const Actions::WindowFocusAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> SetState(Actions::WindowId window, Actions::WindowState state);
  };
}
