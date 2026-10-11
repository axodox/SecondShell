#include "pch.h"
#include "RdpClient.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Protocol;
using namespace std;

namespace SecondShell::Session
{
  RdpClient::RdpClient(dependency_container* container) :
    LoginCompleted(_events),
    Disconnected(_events),
    _credentials(container->resolve<CredentialProvider>())
  { }

  RdpClient::~RdpClient()
  {
    _thread.Invoke([this] { _window.reset(); });
  }

  void RdpClient::Connect(const DisplaySettings& display)
  {
    _thread.Invoke([&] {
      //A fresh control for each connection
      _window.reset();
      _window = make_unique<RdpHostWindow>([this](string_view name, long argument) { OnEvent(name, argument); });

      _window->Configure(display.Width, display.Height, display.Scale);
      _credentials->ApplyTo(_window->Control());
      _window->Connect();
      });

    _logger.log(log_severity::information, "Connecting to the child session at {}x{}, {}%.", display.Width, display.Height, display.Scale);
  }

  void RdpClient::Disconnect()
  {
    _thread.Invoke([this] {
      if (_window) _window->Disconnect();
      });
  }

  void RdpClient::OnEvent(string_view name, long argument)
  {
    if (name == "OnLoginComplete")
    {
      _logger.log(log_severity::information, "Signed in.");
      _events.raise(LoginCompleted, this);
    }
    else if (name == "OnDisconnected")
    {
      _logger.log(log_severity::information, "Disconnected, reason {}.", argument);
      _events.raise(Disconnected, this, argument);
    }
  }
}
