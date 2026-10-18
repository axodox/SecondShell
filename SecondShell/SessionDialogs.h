#pragma once
#include "ActionLog.h"
#include "ElevatedSetupLauncher.h"
#include "ExistingSessionPrompt.h"
#include "SetupChecker.h"

namespace SecondShell::Ui
{
  //The window's questions about the session (docs/UIDesign.md, Dialogs), as WinUI content dialogs. On the UI thread.
  class SessionDialogs
  {
  public:
    //True: reset the child session from before; false: keep it, and cancel the start.
    static winrt::Windows::Foundation::IAsyncOperation<bool> AskToReset(winrt::Microsoft::UI::Xaml::XamlRoot root, Session::ExistingSession session);

    //True: stop the agent, close the window and sign the session out; false: keep it open.
    static winrt::Windows::Foundation::IAsyncOperation<bool> AskToCloseWhileWorking(winrt::Microsoft::UI::Xaml::XamlRoot root, Agent::SequenceLogEntry sequence);

    //The one-time setup: what to set up, or nothing for "Not now".
    static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::IReference<bool>> AskToSetUp(winrt::Microsoft::UI::Xaml::XamlRoot root, Setup::SetupStatus status);
  };
}
