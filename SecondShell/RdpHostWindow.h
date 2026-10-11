#pragma once

namespace SecondShell::Session
{
  //The hidden Win32 window hosting the RDP ActiveX control (MsRdpClient9NotSafeForScripting), hosted with ATL. It is
  //never shown and never minimized: minimizing stops the session from drawing (Findings.md 2.5, 2.11). Created, used
  //and destroyed on one STA thread with a message loop.
  class RdpHostWindow
  {
  public:
    //Receives the control's events (IMsTscAxEvents) by name, with their first integer argument.
    using EventHandler = std::function<void(std::string_view name, long argument)>;

    explicit RdpHostWindow(EventHandler handler);
    ~RdpHostWindow();

    RdpHostWindow(const RdpHostWindow&) = delete;
    RdpHostWindow& operator=(const RdpHostWindow&) = delete;

    IUnknown* Control() const;

    //Everything but the credentials: the child session on this machine, its size and scaling.
    void Configure(uint32_t width, uint32_t height, uint32_t scale);
    void Connect();
    void Disconnect();

  private:
    HWND _window = nullptr;
    CComPtr<IUnknown> _control;
    CComPtr<IConnectionPoint> _connectionPoint;
    DWORD _cookie = 0;
  };
}
