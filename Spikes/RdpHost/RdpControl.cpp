#include "pch.h"
#include "RdpControl.h"

#import "C:\\Windows\\System32\\mstscax.dll" raw_interfaces_only named_guids rename_namespace("Rdp")

namespace
{
  class RdpHostAtlModule : public ATL::CAtlModuleT<RdpHostAtlModule> {};
  RdpHostAtlModule _atlModule;

  //Receives IMsTscAxEvents and reports each event by name (from mstscax's type library) with its first integer argument.
  class EventSink : public IDispatch
  {
    std::atomic<ULONG> _references = 1;
    RdpControl::EventHandler _handler;
    CComPtr<ITypeInfo> _typeInfo;

  public:
    explicit EventSink(RdpControl::EventHandler handler) : _handler(std::move(handler))
    {
      CComPtr<ITypeLib> library;
      if (SUCCEEDED(LoadTypeLibEx(L"mstscax.dll", REGKIND_NONE, &library))) library->GetTypeInfoOfGuid(__uuidof(Rdp::IMsTscAxEvents), &_typeInfo);
    }

    STDMETHODIMP QueryInterface(REFIID iid, void** result) override
    {
      if (iid == IID_IUnknown || iid == IID_IDispatch || iid == __uuidof(Rdp::IMsTscAxEvents))
      {
        *result = static_cast<IDispatch*>(this);
        AddRef();
        return S_OK;
      }
      *result = nullptr;
      return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++_references; }
    STDMETHODIMP_(ULONG) Release() override
    {
      auto count = --_references;
      if (count == 0) delete this;
      return count;
    }

    STDMETHODIMP GetTypeInfoCount(UINT* count) override { *count = 0; return S_OK; }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }

    STDMETHODIMP Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS* parameters, VARIANT*, EXCEPINFO*, UINT*) override
    {
      std::string name = std::format("dispid {}", id);
      if (_typeInfo)
      {
        CComBSTR text;
        UINT count = 0;
        if (SUCCEEDED(_typeInfo->GetNames(id, &text, 1, &count)) && count) name = Spike::Narrow(text.m_str);
      }

      long argument = 0;
      if (parameters && parameters->cArgs > 0)
      {
        //Arguments are in reverse order; the first declared one is last.
        auto& value = parameters->rgvarg[parameters->cArgs - 1];
        if (value.vt == VT_I4) argument = value.lVal;
        else if (value.vt == (VT_I4 | VT_BYREF) && value.plVal) argument = *value.plVal;
      }

      _handler(name, argument);
      return S_OK;
    }
  };

  LRESULT CALLBACK HostWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    return DefWindowProcW(window, message, wParam, lParam);
  }

  void Check(HRESULT result, const char* what)
  {
    if (FAILED(result)) throw std::runtime_error(std::format("{} failed: 0x{:08X}", what, uint32_t(result)));
  }
}

HostWindowMode ParseHostWindowMode(std::wstring_view text)
{
  if (text == L"show-then-hide") return HostWindowMode::ShowThenHide;
  if (text == L"offscreen") return HostWindowMode::Offscreen;
  if (text == L"message") return HostWindowMode::MessageOnly;
  return HostWindowMode::Never;
}

const char* ToString(HostWindowMode mode)
{
  switch (mode)
  {
  case HostWindowMode::ShowThenHide: return "show-then-hide";
  case HostWindowMode::Offscreen: return "offscreen";
  case HostWindowMode::MessageOnly: return "message";
  default: return "never";
  }
}

RdpControl::RdpControl(HostWindowMode mode, int classVersion, EventHandler handler) :
  _mode(mode)
{
  AtlAxWinInit();

  auto instance = GetModuleHandleW(nullptr);
  WNDCLASSW windowClass{ .lpfnWndProc = HostWindowProc, .hInstance = instance, .lpszClassName = L"SecondShellSpikeRdpHost" };
  RegisterClassW(&windowClass);

  int x = mode == HostWindowMode::Offscreen ? -5000 : 100;
  int y = mode == HostWindowMode::Offscreen ? -5000 : 100;
  HWND parent = mode == HostWindowMode::MessageOnly ? HWND_MESSAGE : nullptr;
  DWORD style = mode == HostWindowMode::MessageOnly ? 0 : WS_OVERLAPPEDWINDOW;
  _window = CreateWindowExW(WS_EX_NOACTIVATE, windowClass.lpszClassName, L"SecondShell spike RDP host", style | WS_CLIPCHILDREN, x, y, 1040, 810, parent, nullptr, instance, nullptr);
  if (!_window) throw std::runtime_error(std::format("CreateWindowEx failed: {}", GetLastError()));

  RECT client;
  GetClientRect(_window, &client);
  if (client.right == 0) client = { 0, 0, 1024, 768 };
  _axWindow = CreateWindowExW(0, CAxWindow::GetWndClassName(), L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, client.right, client.bottom, _window, nullptr, instance, nullptr);
  if (!_axWindow) throw std::runtime_error(std::format("Creating the AtlAxWin window failed: {}", GetLastError()));

  wchar_t clsid[64];
  StringFromGUID2(classVersion == 7 ? __uuidof(Rdp::MsRdpClient7NotSafeForScripting) : __uuidof(Rdp::MsRdpClient9NotSafeForScripting), clsid, 64);
  CAxWindow ax(_axWindow);
  Check(ax.CreateControlEx(clsid, nullptr, nullptr, &_control), "CreateControlEx(MsRdpClientNNotSafeForScripting)");

  CComQIPtr<IConnectionPointContainer> container(_control);
  Check(container->FindConnectionPoint(__uuidof(Rdp::IMsTscAxEvents), &_connectionPoint), "FindConnectionPoint");
  auto sink = new EventSink(std::move(handler));
  Check(_connectionPoint->Advise(sink, &_cookie), "Advise");
  sink->Release();

  if (mode == HostWindowMode::ShowThenHide || mode == HostWindowMode::Offscreen) ShowWindow(_window, SW_SHOWNOACTIVATE);
}

RdpControl::~RdpControl()
{
  if (_connectionPoint && _cookie) _connectionPoint->Unadvise(_cookie);
  _connectionPoint.Release();
  _control.Release();
  if (_window) DestroyWindow(_window);
}

void RdpControl::Connect(const std::wstring& user, const std::wstring& domain, const std::wstring& password, int width, int height)
{
  CComQIPtr<Rdp::IMsRdpClient7> client(_control);
  if (!client) throw std::runtime_error("IMsRdpClient9 not supported");

  Check(client->put_Server(CComBSTR(L"localhost")), "Server");
  Check(client->put_UserName(CComBSTR(user.c_str())), "UserName");
  Check(client->put_Domain(CComBSTR(domain.c_str())), "Domain");
  Check(client->put_DesktopWidth(width), "DesktopWidth");
  Check(client->put_DesktopHeight(height), "DesktopHeight");

  CComPtr<Rdp::IMsTscAdvancedSettings> settings;
  Check(client->get_AdvancedSettings(&settings), "AdvancedSettings");
  CComQIPtr<Rdp::IMsRdpClientAdvancedSettings7> advanced(settings);
  CComBSTR secret(password.c_str());
  Check(advanced->put_ClearTextPassword(secret), "ClearTextPassword");
  SecureZeroMemory(secret.m_str, secret.ByteLength());
  Check(advanced->put_EnableCredSspSupport(VARIANT_TRUE), "EnableCredSspSupport");
  Check(advanced->put_RedirectClipboard(VARIANT_FALSE), "RedirectClipboard");

  CComQIPtr<Rdp::IMsRdpExtendedSettings> extended(_control);
  CComVariant on(true);
  Check(extended->put_Property(CComBSTR(L"ConnectToChildSession"), &on), "ConnectToChildSession");

  CComQIPtr<Rdp::IMsRdpClientNonScriptable5> nonScriptable(_control);
  Check(nonScriptable->put_AllowPromptingForCredentials(VARIANT_FALSE), "AllowPromptingForCredentials");
  Check(nonScriptable->put_PromptForCredentials(VARIANT_FALSE), "PromptForCredentials");

  Check(client->Connect(), "Connect");
}

void RdpControl::Disconnect()
{
  CComQIPtr<Rdp::IMsRdpClient7> client(_control);
  if (client) client->Disconnect();
}

void RdpControl::OnLoginComplete()
{
  if (_mode == HostWindowMode::ShowThenHide) ShowWindow(_window, SW_HIDE);
}

bool RdpControl::IsConnected() const
{
  CComQIPtr<Rdp::IMsRdpClient7> client(_control);
  short connected = 0;
  if (client) client->get_Connected(&connected);
  return connected == 1;
}

std::string RdpControl::WindowState() const
{
  RECT rect{};
  GetWindowRect(_window, &rect);
  return std::format("visible={} iconic={} WS_VISIBLE={} rect=({},{})-({},{}) foreground={}",
    IsWindowVisible(_window) != 0, IsIconic(_window) != 0, (GetWindowLongW(_window, GWL_STYLE) & WS_VISIBLE) != 0,
    rect.left, rect.top, rect.right, rect.bottom, GetForegroundWindow() == _window);
}

