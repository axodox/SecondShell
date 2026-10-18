#include "pch.h"
#include "ClipboardSync.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace SecondShell::Desktop;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;

namespace SecondShell::Agent
{
  namespace
  {
    template<typename T>
    void ThrowIfFailed(const RpcResult<T>& result)
    {
      if (auto error = get_if<JsonRpcError>(&result)) throw runtime_error(error->Message);
    }
  }

  ClipboardSync::ClipboardSync(dependency_container* container) :
    _driver(container->resolve<DriverLink>()),
    _agentControl(container->resolve<AgentControl>()),
    _window(container->resolve<MessageWindow>()),
    _mirror(container->resolve<ClipboardMirror>()),
    _modeChangedSubscription(_agentControl->ModeChanged({ this, &ClipboardSync::OnModeChanged })),
    _userClipboardChangedSubscription(_mirror->Changed({ this, &ClipboardSync::OnUserClipboardChanged })),
    _sessionClipboardChangedSubscription(_driver->ClipboardChanged({ this, &ClipboardSync::OnSessionClipboardChanged }))
  { }

  void ClipboardSync::Share(bool isEnabled)
  {
    _mirror->IsWatching(isEnabled);
    if (!_driver->IsConnected()) return;

    ThrowIfFailed(_driver->SetClipboardSync(isEnabled));
    if (isEnabled) SendToSession(_mirror->Read());

    _logger.log(log_severity::information, "Clipboard sharing {}.", isEnabled ? "on" : "off");
  }

  void ClipboardSync::SendToSession(const vector<ClipboardContent>& contents)
  {
    if (!contents.empty()) ThrowIfFailed(_driver->SetClipboard(contents));
  }

  void ClipboardSync::OnModeChanged(AgentControl* /*sender*/, ControlMode mode)
  {
    _window->Post([this, isEnabled = mode == ControlMode::User] { Share(isEnabled); });
  }

  void ClipboardSync::OnUserClipboardChanged(ClipboardMirror* /*sender*/, const vector<ClipboardContent>& contents)
  {
    //Already on the window's thread
    if (_agentControl->Mode() == ControlMode::User) SendToSession(contents);
  }

  void ClipboardSync::OnSessionClipboardChanged(DriverLink* /*sender*/, const vector<ClipboardContent>& contents)
  {
    if (_agentControl->Mode() != ControlMode::User) return;
    _window->Post([this, contents] { _mirror->Write(contents); });
  }
}
