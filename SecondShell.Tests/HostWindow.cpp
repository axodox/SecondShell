#include "pch.h"
#include "HostWindow.h"
#include "AppEnvironment.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Environment;
using namespace std;

namespace SecondShell::Tests
{
  namespace
  {
    optional<DWORD> HostProcessId()
    {
      auto snapshot = winrt::handle{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
      PROCESSENTRY32W entry{ .dwSize = sizeof(PROCESSENTRY32W) };
      for (auto found = Process32FirstW(snapshot.get(), &entry); found; found = Process32NextW(snapshot.get(), &entry))
      {
        if (_wcsicmp(entry.szExeFile, AppEnvironment().HostExecutable().filename().c_str()) == 0) return entry.th32ProcessID;
      }
      return nullopt;
    }
  }

  HostWindow::HostWindow()
  {
    auto processId = HostProcessId();
    Assert::IsTrue(processId.has_value(), L"the host should be running");

    struct Search
    {
      DWORD ProcessId;
      HWND Window;
    } search{ *processId, nullptr };

    EnumWindows([](HWND window, LPARAM context) {
      auto& search = *reinterpret_cast<Search*>(context);
      DWORD windowProcessId = 0;
      GetWindowThreadProcessId(window, &windowProcessId);
      if (windowProcessId != search.ProcessId || !IsWindowVisible(window)) return TRUE;

      search.Window = window;
      return FALSE;
      }, LPARAM(&search));

    Assert::IsNotNull(search.Window, L"the host should show its window");
    _handle = search.Window;
  }

  HWND HostWindow::Handle() const
  {
    return _handle;
  }

  void HostWindow::Click(wstring_view name) const
  {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto automation = winrt::create_instance<IUIAutomation>(__uuidof(CUIAutomation));

    winrt::com_ptr<IUIAutomationElement> window;
    winrt::check_hresult(automation->ElementFromHandle(_handle, window.put()));

    winrt::com_ptr<IUIAutomationCondition> condition;
    winrt::check_hresult(automation->CreatePropertyCondition(UIA_NamePropertyId, _variant_t(wstring(name).c_str()), condition.put()));

    winrt::com_ptr<IUIAutomationElement> button;
    winrt::check_hresult(window->FindFirst(TreeScope_Descendants, condition.get(), button.put()));
    Assert::IsNotNull(button.get(), format(L"the window should have a {} button", name).c_str());

    winrt::com_ptr<IUIAutomationInvokePattern> invoke;
    winrt::check_hresult(button->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(invoke.put())));
    winrt::check_hresult(invoke->Invoke());
  }
}
