#include "pch.h"
#include "RdpClient.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Protocol;
using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    uint32_t OrientationDegrees(DisplayOrientation orientation)
    {
      switch (orientation)
      {
      case DisplayOrientation::Portrait:
        return 90;
      case DisplayOrientation::LandscapeFlipped:
        return 180;
      case DisplayOrientation::PortraitFlipped:
        return 270;
      default:
        return 0;
      }
    }
  }

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
      _credentials->ApplyTo(*_window);
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

  void RdpClient::UpdateDisplay(const DisplaySettings& display)
  {
    _thread.Invoke([&] {
      if (!_window) throw runtime_error("There is no connection to the child session.");
      _window->UpdateDisplay(display.Width, display.Height, display.Scale, OrientationDegrees(display.Orientation));
      });

    _logger.log(log_severity::information, "Changing the display to {}x{}, {}%, {}.", display.Width, display.Height, display.Scale, to_string(display.Orientation));
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
