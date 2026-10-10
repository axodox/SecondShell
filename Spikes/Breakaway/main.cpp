//Starts a program outside the MSIX desktop app runtime environment (package context) of the calling process.
//Processes started by a packaged app (e.g. the Claude desktop app, or any packaged AI client) inherit its registry and
//AppData write virtualization; the SecondShell host must not, or its RunOnce entry never reaches Explorer.
//
//Usage:
//  Breakaway.exe --policy N <command line...>      CreateProcess with PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY = N, waits
//  Breakaway.exe --explorer <file> [arguments]     asks the running Explorer to start it (IShellDispatch2::ShellExecute)
//  Breakaway.exe --explorer <file> @<path>         same, with the arguments read from a UTF-8 file
#include <windows.h>
#include <shlobj.h>
#include <shldisp.h>
#include <exdisp.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace
{
  //The rest of our own command line, starting at argv[index], verbatim.
  std::wstring CommandLineFrom(wchar_t** argv, int index)
  {
    std::wstring commandLine = GetCommandLineW();
    std::wstring target = argv[index];
    auto position = commandLine.find(target);
    if (position != std::wstring::npos && position > 0 && commandLine[position - 1] == L'"') position--;
    return position == std::wstring::npos ? target : commandLine.substr(position);
  }

  int RunWithPolicy(DWORD policy, std::wstring commandLine)
  {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<uint8_t> buffer(size);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
    InitializeProcThreadAttributeList(attributes, 1, 0, &size);
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY, &policy, sizeof(policy), nullptr, nullptr))
    {
      fwprintf(stderr, L"UpdateProcThreadAttribute failed: %lu\n", GetLastError());
      return 3;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process))
    {
      fwprintf(stderr, L"CreateProcess failed: %lu\n", GetLastError());
      return 4;
    }
    DeleteProcThreadAttributeList(attributes);

    DWORD exitCode = 0;
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return int(exitCode);
  }

  //Raymond Chen's method: find the desktop's shell view in the running Explorer and call ShellExecute through it,
  //so the program is started by Explorer, in Explorer's (unpackaged) context.
  int RunViaExplorer(const wchar_t* file, const std::wstring& arguments)
  {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT result;
    IShellWindows* windows = nullptr;
    result = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&windows));
    if (FAILED(result)) { fwprintf(stderr, L"ShellWindows: 0x%08X\n", result); return 5; }

    VARIANT location{}, empty{};
    location.vt = VT_I4;
    location.lVal = CSIDL_DESKTOP;
    long hwnd = 0;
    IDispatch* dispatch = nullptr;
    result = windows->FindWindowSW(&location, &empty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &dispatch);
    if (result != S_OK || !dispatch) { fwprintf(stderr, L"FindWindowSW: 0x%08X\n", result); return 6; }

    IServiceProvider* services = nullptr;
    IShellBrowser* browser = nullptr;
    IShellView* view = nullptr;
    IDispatch* background = nullptr;
    IShellFolderViewDual* folderView = nullptr;
    IDispatch* application = nullptr;
    IShellDispatch2* shell = nullptr;
    result = dispatch->QueryInterface(IID_PPV_ARGS(&services));
    if (SUCCEEDED(result)) result = services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
    if (SUCCEEDED(result)) result = browser->QueryActiveShellView(&view);
    if (SUCCEEDED(result)) result = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&background));
    if (SUCCEEDED(result)) result = background->QueryInterface(IID_PPV_ARGS(&folderView));
    if (SUCCEEDED(result)) result = folderView->get_Application(&application);
    if (SUCCEEDED(result)) result = application->QueryInterface(IID_PPV_ARGS(&shell));
    if (FAILED(result)) { fwprintf(stderr, L"Getting IShellDispatch2: 0x%08X\n", result); return 7; }

    VARIANT args{}, directory{}, operation{}, show{};
    args.vt = VT_BSTR;
    args.bstrVal = SysAllocString(arguments.c_str());
    show.vt = VT_I4;
    show.lVal = SW_SHOWNOACTIVATE;
    auto fileName = SysAllocString(file);
    result = shell->ShellExecute(fileName, args, directory, operation, show);
    if (FAILED(result)) { fwprintf(stderr, L"ShellExecute: 0x%08X\n", result); return 8; }
    SysFreeString(fileName);
    VariantClear(&args);

    shell->Release(); application->Release(); folderView->Release(); background->Release();
    view->Release(); browser->Release(); services->Release(); dispatch->Release(); windows->Release();
    CoUninitialize();
    return 0;
  }
}

int wmain(int argc, wchar_t** argv)
{
  if (argc >= 4 && std::wstring(argv[1]) == L"--policy") return RunWithPolicy(std::wcstoul(argv[2], nullptr, 0), CommandLineFrom(argv, 3));
  if (argc >= 3 && std::wstring(argv[1]) == L"--explorer")
  {
    std::wstring arguments = argc >= 4 ? CommandLineFrom(argv, 3) : L"";
    if (argc == 4 && argv[3][0] == L'@')
    {
      std::ifstream file(argv[3] + 1, std::ios::binary);
      std::stringstream text;
      text << file.rdbuf();
      auto utf8 = text.str();
      arguments.resize(MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0));
      MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), arguments.data(), int(arguments.size()));
    }
    return RunViaExplorer(argv[2], arguments);
  }

  fwprintf(stderr, L"Usage: Breakaway.exe --policy N <command line> | --explorer <file> [arguments]\n");
  return 2;
}

