#pragma once

namespace SecondShell::Session
{
  //Where an RDP connection goes.
  enum class RdpTarget
  {
    ChildSession,  //the agent's desktop
    OwnSession     //the user's own session, through the loopback address, to keep it unlocked behind the lock screen
  };

  //The hidden Win32 window hosting the RDP ActiveX control (MsRdpClient9NotSafeForScripting), hosted with ATL. It is
  //never shown and never minimized: minimizing stops the session from drawing (Findings.md 2.5, 2.11). The control has
  //no UI of its own beyond this window, so no dialog ever appears, not even when a connection ends. Created, used and
  //destroyed on one STA thread with a message loop.
  class RdpHostWindow
  {
  public:
    //Receives the control's events (IMsTscAxEvents) by name, with their first integer argument.
    using EventHandler = std::function<void(std::string_view name, long argument)>;

    explicit RdpHostWindow(EventHandler handler);
    ~RdpHostWindow();

    RdpHostWindow(const RdpHostWindow&) = delete;
    RdpHostWindow& operator=(const RdpHostWindow&) = delete;

    //Everything but the credentials: the target on this machine, the session's size and scaling.
    void Configure(RdpTarget target, uint32_t width, uint32_t height, uint32_t scale);
    void SetCredentials(std::wstring_view userName, std::wstring_view domain, std::wstring_view password);
    void Connect();
    void Disconnect();

    //Changes the size, scaling and orientation (in degrees) of the connected session.
    void UpdateDisplay(uint32_t width, uint32_t height, uint32_t scale, uint32_t orientation);

  private:
    HWND _window = nullptr;
    CComPtr<IUnknown> _control;
    CComPtr<IConnectionPoint> _connectionPoint;
    DWORD _cookie = 0;
  };
}
