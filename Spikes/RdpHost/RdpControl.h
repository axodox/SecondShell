#pragma once

//How the top-level window owning the RDP control is treated.
enum class HostWindowMode
{
  Never,        //created without WS_VISIBLE and never shown (the design)
  ShowThenHide, //shown without activation until sign-in completes, then hidden (Findings.md 2.5)
  Offscreen,    //shown without activation at (-5000, -5000)
  MessageOnly   //parented to HWND_MESSAGE
};

HostWindowMode ParseHostWindowMode(std::wstring_view text);
const char* ToString(HostWindowMode mode);

//The RDP ActiveX control (MsRdpClient7/9NotSafeForScripting) in its own top-level Win32 window, hosted with ATL.
//Must be created, used and destroyed on one STA thread that pumps messages.
class RdpControl
{
public:
  using EventHandler = std::function<void(const std::string& name, long argument)>;

  RdpControl(HostWindowMode mode, int classVersion, EventHandler handler);  //classVersion: 7 or 9 (MsRdpClientNNotSafeForScripting)
  ~RdpControl();

  void Connect(const std::wstring& user, const std::wstring& domain, const std::wstring& password, int width, int height);
  void Disconnect();
  void OnLoginComplete();

  bool IsConnected() const;
  HWND Window() const { return _window; }
  std::string WindowState() const;

private:
  HostWindowMode _mode;
  HWND _window = nullptr;
  HWND _axWindow = nullptr;
  CComPtr<IUnknown> _control;
  CComPtr<IConnectionPoint> _connectionPoint;
  DWORD _cookie = 0;
};

