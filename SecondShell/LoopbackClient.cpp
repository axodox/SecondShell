#include "pch.h"
#include "LoopbackClient.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace SecondShell::Session
{
  LoopbackClient::LoopbackClient(dependency_container* container) :
    LoginCompleted(_events),
    Disconnected(_events),
    _credentials(container->resolve<CredentialProvider>())
  { }

  LoopbackClient::~LoopbackClient()
  {
    _thread.Invoke([this] {
      if (_window) _window->Disconnect();
      _window.reset();
      });
  }

  bool LoopbackClient::Connect()
  {
    //The main display's size and scaling, so the user's windows keep their places as far as they can
    auto width = uint32_t(GetSystemMetrics(SM_CXSCREEN));
    auto height = uint32_t(GetSystemMetrics(SM_CYSCREEN));
    auto scale = clamp(uint32_t(GetDpiForSystem() * 100 / USER_DEFAULT_SCREEN_DPI), 100u, 500u);

    auto hasCredentials = false;
    _thread.Invoke([&] {
      //A fresh control for each connection
      _window.reset();
      _window = make_unique<RdpHostWindow>([this](string_view name, long argument) { OnEvent(name, argument); });

      _window->Configure(RdpTarget::OwnSession, width, height, scale);
      hasCredentials = _credentials->ApplySavedTo(*_window);
      if (!hasCredentials)
      {
        _window.reset();
        return;
      }

      _window->Connect();
      });

    if (hasCredentials) _logger.log(log_severity::information, "Connecting to this PC's own session at {}x{}, {}%.", width, height, scale);
    return hasCredentials;
  }

  void LoopbackClient::Disconnect()
  {
    _thread.Invoke([this] {
      if (_window) _window->Disconnect();
      });
  }

  void LoopbackClient::Release()
  {
    _thread.Invoke([this] { _window.reset(); });
  }

  void LoopbackClient::OnEvent(string_view name, long argument)
  {
    if (name == "OnLoginComplete")
    {
      _logger.log(log_severity::information, "Signed in: the session is unlocked behind the lock screen.");
      _events.raise(LoginCompleted, this);
    }
    else if (name == "OnDisconnected")
    {
      _logger.log(log_severity::information, "Disconnected, reason {}.", argument);
      _events.raise(Disconnected, this, argument);
    }
  }
}
