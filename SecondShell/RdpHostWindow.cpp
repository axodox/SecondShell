#include "pch.h"
#include "RdpHostWindow.h"

#import "C:\\Windows\\System32\\mstscax.dll" raw_interfaces_only named_guids rename_namespace("Rdp")

using namespace std;

namespace SecondShell::Session
{
  namespace
  {
    const wchar_t* const _windowClassName = L"SecondShell.RdpHost";

    class HostAtlModule : public ATL::CAtlModuleT<HostAtlModule> {};
    HostAtlModule _atlModule;

    //Receives IMsTscAxEvents and reports each event by its name in mstscax's type library.
    class EventSink : public IDispatch
    {
    public:
      explicit EventSink(RdpHostWindow::EventHandler handler) :
        _handler(move(handler))
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

      STDMETHODIMP_(ULONG) AddRef() override
      {
        return ++_references;
      }

      STDMETHODIMP_(ULONG) Release() override
      {
        auto count = --_references;
        if (count == 0) delete this;
        return count;
      }

      STDMETHODIMP GetTypeInfoCount(UINT* count) override
      {
        *count = 0;
        return S_OK;
      }

      STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override
      {
        return E_NOTIMPL;
      }

      STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override
      {
        return E_NOTIMPL;
      }

      STDMETHODIMP Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS* parameters, VARIANT*, EXCEPINFO*, UINT*) override
      {
        string name = format("dispid {}", id);
        CComBSTR text;
        UINT count = 0;
        if (_typeInfo && SUCCEEDED(_typeInfo->GetNames(id, &text, 1, &count)) && count) name = winrt::to_string(text.m_str);

        //Arguments are in reverse order: the first declared one is the last
        long argument = 0;
        if (parameters && parameters->cArgs > 0)
        {
          auto& value = parameters->rgvarg[parameters->cArgs - 1];
          if (value.vt == VT_I4) argument = value.lVal;
          else if (value.vt == (VT_I4 | VT_BYREF) && value.plVal) argument = *value.plVal;
        }

        _handler(name, argument);
        return S_OK;
      }

    private:
      atomic<ULONG> _references = 1;
      RdpHostWindow::EventHandler _handler;
      CComPtr<ITypeInfo> _typeInfo;
    };
  }

  RdpHostWindow::RdpHostWindow(EventHandler handler)
  {
    AtlAxWinInit();

    //The window is created without WS_VISIBLE and never shown
    auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{ .lpfnWndProc = DefWindowProcW, .hInstance = instance, .lpszClassName = _windowClassName };
    RegisterClassW(&windowClass);

    _window = CreateWindowExW(WS_EX_NOACTIVATE, _windowClassName, L"SecondShell RDP host", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 0, 0, 1024, 768, nullptr, nullptr, instance, nullptr);
    winrt::check_bool(_window != nullptr);

    RECT client;
    GetClientRect(_window, &client);
    auto axWindow = CreateWindowExW(0, CAxWindow::GetWndClassName(), L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, client.right, client.bottom, _window, nullptr, instance, nullptr);
    winrt::check_bool(axWindow != nullptr);

    wchar_t classId[64];
    StringFromGUID2(__uuidof(Rdp::MsRdpClient9NotSafeForScripting), classId, 64);
    CAxWindow ax(axWindow);
    winrt::check_hresult(ax.CreateControlEx(classId, nullptr, nullptr, &_control));

    //Events
    CComQIPtr<IConnectionPointContainer> container(_control);
    winrt::check_hresult(container->FindConnectionPoint(__uuidof(Rdp::IMsTscAxEvents), &_connectionPoint));
    auto sink = new EventSink(move(handler));
    auto result = _connectionPoint->Advise(sink, &_cookie);
    sink->Release();
    winrt::check_hresult(result);
  }

  RdpHostWindow::~RdpHostWindow()
  {
    if (_connectionPoint && _cookie) _connectionPoint->Unadvise(_cookie);
    _connectionPoint.Release();
    _control.Release();
    if (_window) DestroyWindow(_window);
  }

  void RdpHostWindow::Configure(uint32_t width, uint32_t height, uint32_t scale)
  {
    CComQIPtr<Rdp::IMsRdpClient9> client(_control);
    winrt::check_hresult(client->put_Server(CComBSTR(L"localhost")));
    winrt::check_hresult(client->put_DesktopWidth(width));
    winrt::check_hresult(client->put_DesktopHeight(height));

    CComPtr<Rdp::IMsTscAdvancedSettings> settings;
    winrt::check_hresult(client->get_AdvancedSettings(&settings));
    CComQIPtr<Rdp::IMsRdpClientAdvancedSettings8> advanced(settings);
    winrt::check_hresult(advanced->put_EnableCredSspSupport(VARIANT_TRUE));
    winrt::check_hresult(advanced->put_RedirectClipboard(VARIANT_FALSE));

    //The child session, with the session's own display scaling
    CComQIPtr<Rdp::IMsRdpExtendedSettings> extended(_control);
    CComVariant isChildSession{ true }, desktopScale{ ULONG(scale) }, deviceScale{ ULONG(100) };
    winrt::check_hresult(extended->put_Property(CComBSTR(L"ConnectToChildSession"), &isChildSession));
    winrt::check_hresult(extended->put_Property(CComBSTR(L"DesktopScaleFactor"), &desktopScale));
    winrt::check_hresult(extended->put_Property(CComBSTR(L"DeviceScaleFactor"), &deviceScale));

    //Never a credential dialog of the control's own
    CComQIPtr<Rdp::IMsRdpClientNonScriptable5> nonScriptable(_control);
    winrt::check_hresult(nonScriptable->put_AllowPromptingForCredentials(VARIANT_FALSE));
    winrt::check_hresult(nonScriptable->put_PromptForCredentials(VARIANT_FALSE));
  }

  void RdpHostWindow::SetCredentials(wstring_view userName, wstring_view domain, wstring_view password)
  {
    CComQIPtr<Rdp::IMsRdpClient9> client(_control);
    winrt::check_hresult(client->put_UserName(CComBSTR(int(userName.size()), userName.data())));
    winrt::check_hresult(client->put_Domain(CComBSTR(int(domain.size()), domain.data())));

    CComPtr<Rdp::IMsTscAdvancedSettings> settings;
    winrt::check_hresult(client->get_AdvancedSettings(&settings));
    CComQIPtr<Rdp::IMsRdpClientAdvancedSettings8> advanced(settings);

    CComBSTR secret(int(password.size()), password.data());
    auto result = advanced->put_ClearTextPassword(secret);
    SecureZeroMemory(secret.m_str, secret.ByteLength());
    winrt::check_hresult(result);
  }

  void RdpHostWindow::Connect()
  {
    CComQIPtr<Rdp::IMsRdpClient9> client(_control);
    winrt::check_hresult(client->Connect());
  }

  void RdpHostWindow::Disconnect()
  {
    CComQIPtr<Rdp::IMsRdpClient9> client(_control);
    short isConnected = 0;
    if (SUCCEEDED(client->get_Connected(&isConnected)) && isConnected) client->Disconnect();
  }
}
