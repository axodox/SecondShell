#pragma once
#include "ActionLog.h"
#include "ExistingSessionPrompt.h"

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
  };
}
