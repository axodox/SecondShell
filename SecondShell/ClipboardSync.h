#pragma once
#include "AgentControl.h"
#include "ClipboardMirror.h"
#include "DriverLink.h"

namespace SecondShell::Agent
{
  //The user's side of the clipboard sharing (Design.md, Clipboard management): while the user has control, the session
  //gets the user's clipboard, at once and on each change, and the user gets the session's changes. Otherwise the two
  //clipboards stay apart. The work runs on the clipboard's window thread, in order, never on the UI thread.
  class ClipboardSync
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ClipboardSync" };

  public:
    explicit ClipboardSync(Axodox::Infrastructure::dependency_container* container);

  private:
    std::shared_ptr<Session::DriverLink> _driver;
    std::shared_ptr<AgentControl> _agentControl;
    std::shared_ptr<Desktop::MessageWindow> _window;
    std::shared_ptr<Desktop::ClipboardMirror> _mirror;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    Axodox::Infrastructure::event_subscription _userClipboardChangedSubscription;
    Axodox::Infrastructure::event_subscription _sessionClipboardChangedSubscription;

    void Share(bool isEnabled);
    void SendToSession(const std::vector<Actions::ClipboardContent>& contents);

    void OnModeChanged(AgentControl* sender, ControlMode mode);
    void OnUserClipboardChanged(Desktop::ClipboardMirror* sender, const std::vector<Actions::ClipboardContent>& contents);
    void OnSessionClipboardChanged(Session::DriverLink* sender, const std::vector<Actions::ClipboardContent>& contents);
  };
}
