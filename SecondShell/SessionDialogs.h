#pragma once
#include "ActionLog.h"
#include "ElevatedSetupLauncher.h"
#include "AiClientIntegration.h"
#include "ExistingSessionPrompt.h"
#include "SetupChecker.h"
#include "Uninstaller.h"

namespace SecondShell::Ui
{
  //The window's questions (docs/UIDesign.md, Dialogs), as WinUI content dialogs. On the UI thread.
  class SessionDialogs
  {
  public:
    //True: reset the child session from before; false: keep it, and cancel the start.
    static winrt::Windows::Foundation::IAsyncOperation<bool> AskToReset(winrt::Microsoft::UI::Xaml::XamlRoot root, Session::ExistingSession session);

    //True: stop the agent, close the window and sign the session out; false: keep it open.
    static winrt::Windows::Foundation::IAsyncOperation<bool> AskToCloseWhileWorking(winrt::Microsoft::UI::Xaml::XamlRoot root, Agent::SequenceLogEntry sequence);

    //The one-time setup: what to set up, or nothing for "Not now".
    static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::IReference<bool>> AskToSetUp(winrt::Microsoft::UI::Xaml::XamlRoot root, Setup::SetupStatus status);

    //The integration preview: the change to an AI app's configuration before it is made. True: make it.
    //What to remove besides the AI apps' entries: nothing for Cancel, else flags: 1 the password, 2 the data folder.
    static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::IReference<int32_t>> AskToUninstall(winrt::Microsoft::UI::Xaml::XamlRoot root);
    static winrt::Windows::Foundation::IAsyncAction ShowUninstallReport(winrt::Microsoft::UI::Xaml::XamlRoot root, Setup::UninstallReport report);

    static winrt::Windows::Foundation::IAsyncOperation<bool> AskToChangeIntegration(winrt::Microsoft::UI::Xaml::XamlRoot root, winrt::hstring title, winrt::hstring action, Integrations::IntegrationChange change);
  };
}
