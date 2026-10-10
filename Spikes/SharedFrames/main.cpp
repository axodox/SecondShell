//Spike 2, writer side, run inside the child session: captures the session with Windows.Graphics.Capture, copies each
//frame into a shared texture (NT handle, keyed mutex) and duplicates the texture and a metadata section into the host
//process in the user's session. The host (RdpHost --frames) reads the frames and measures rate and latency.
//An animated counter window keeps the screen changing, so that capture produces frames continuously.
#include "SpikeCommon.h"
#include <atomic>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace
{
  std::atomic<uint64_t> _counter = 0;

  LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case WM_TIMER:
      _counter++;
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_PAINT:
    {
      PAINTSTRUCT paint;
      auto dc = BeginPaint(window, &paint);
      RECT rect;
      GetClientRect(window, &rect);
      auto value = _counter.load();
      auto brush = CreateSolidBrush(RGB(value * 7 & 0xFF, 0x30, 0x80));
      FillRect(dc, &rect, brush);
      DeleteObject(brush);
      auto font = CreateFontW(64, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
      auto old = SelectObject(dc, font);
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, RGB(255, 255, 255));
      auto text = std::format(L"{:06}", value);
      DrawTextW(dc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old);
      DeleteObject(font);
      EndPaint(window, &paint);
      return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
  }

  std::string Describe(IDXGIAdapter1* adapter)
  {
    DXGI_ADAPTER_DESC1 description;
    adapter->GetDesc1(&description);
    return std::format("{} luid {:08X}:{:08X}", Spike::Narrow(description.Description), uint32_t(description.AdapterLuid.HighPart), description.AdapterLuid.LowPart);
  }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
  Spike::Logger log(Spike::OutDir() / L"child.log");
  Spike::SessionGuard(log);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  winrt::init_apartment(winrt::apartment_type::multi_threaded);

  int argc;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  auto seconds = std::stoi(Spike::ArgValue(argc, argv, L"--seconds", L"30"));
  auto writerAdapterIndex = std::stoi(Spike::ArgValue(argc, argv, L"--writer-adapter", L"-1")); //-1: the host's adapter

  try
  {
    //Adapters and outputs as seen in the child session
    winrt::com_ptr<IDXGIFactory6> factory;
    winrt::check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(factory.put())));
    winrt::com_ptr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, adapter.put()) != DXGI_ERROR_NOT_FOUND; i++)
    {
      log(std::format("Adapter {}: {}", i, Describe(adapter.get())));
      winrt::com_ptr<IDXGIOutput> output;
      for (UINT j = 0; adapter->EnumOutputs(j, output.put()) != DXGI_ERROR_NOT_FOUND; j++)
      {
        DXGI_OUTPUT_DESC description;
        output->GetDesc(&description);
        log(std::format("  output {}: {} ({},{})-({},{})", j, Spike::Narrow(description.DeviceName), description.DesktopCoordinates.left, description.DesktopCoordinates.top, description.DesktopCoordinates.right, description.DesktopCoordinates.bottom));
        output = nullptr;
      }
      adapter = nullptr;
    }

    //The host's process and adapter
    std::map<std::string, std::string> hostInfo;
    for (int i = 0; i < 40 && hostInfo.empty(); i++)
    {
      hostInfo = Spike::ReadKeyValues(Spike::OutDir() / L"host.txt");
      if (hostInfo.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    if (hostInfo.empty()) throw std::runtime_error("host.txt missing");
    auto hostPid = DWORD(std::stoul(hostInfo["host_pid"]));
    LUID luid{ DWORD(std::stoul(hostInfo["adapter_luid_low"])), LONG(std::stol(hostInfo["adapter_luid_high"])) };

    if (writerAdapterIndex >= 0) winrt::check_hresult(factory->EnumAdapters1(writerAdapterIndex, adapter.put()));
    else winrt::check_hresult(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(adapter.put())));
    auto adapterName = Describe(adapter.get());
    log(std::format("Host pid {}, host adapter '{}', writer adapter {}", hostPid, hostInfo["adapter_name"], adapterName));

    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    winrt::check_hresult(D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put()));
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(device.as<IDXGIDevice>().get(), inspectable.put()));
    auto direct3dDevice = inspectable.as<IDirect3DDevice>();

    //Animated window
    WNDCLASSW windowClass{ .lpfnWndProc = WindowProc, .hInstance = instance, .lpszClassName = L"SpikeSharedFramesCounter" };
    RegisterClassW(&windowClass);
    auto window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, windowClass.lpszClassName, L"SharedFrames", WS_POPUP | WS_VISIBLE, 60, 60, 360, 160, nullptr, nullptr, instance, nullptr);
    SetTimer(window, 1, 16, nullptr);

    //Capture of the primary monitor
    log(std::format("GraphicsCaptureSession.IsSupported: {}", GraphicsCaptureSession::IsSupported()));
    auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    GraphicsCaptureItem item{ nullptr };
    winrt::check_hresult(interop->CreateForMonitor(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), winrt::guid_of<GraphicsCaptureItem>(), winrt::put_abi(item)));
    auto size = item.Size();
    log(std::format("Capture item '{}' {}x{}", winrt::to_string(item.DisplayName()), size.Width, size.Height));

    //Shared texture and metadata section, duplicated into the host
    D3D11_TEXTURE2D_DESC description{};
    description.Width = size.Width;
    description.Height = size.Height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    description.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    winrt::com_ptr<ID3D11Texture2D> shared;
    winrt::check_hresult(device->CreateTexture2D(&description, nullptr, shared.put()));
    auto mutex = shared.as<IDXGIKeyedMutex>();
    HANDLE textureHandle = nullptr;
    winrt::check_hresult(shared.as<IDXGIResource1>()->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &textureHandle));

    auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
    auto meta = static_cast<Spike::SharedFrameMeta*>(MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Spike::SharedFrameMeta)));
    *meta = {};

    auto hostProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, hostPid);
    if (!hostProcess) throw std::runtime_error(std::format("OpenProcess(PROCESS_DUP_HANDLE, host {}) failed: {}", hostPid, GetLastError()));
    HANDLE remoteTexture = nullptr, remoteSection = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), textureHandle, hostProcess, &remoteTexture, 0, FALSE, DUPLICATE_SAME_ACCESS)) throw std::runtime_error(std::format("DuplicateHandle(texture) failed: {}", GetLastError()));
    if (!DuplicateHandle(GetCurrentProcess(), section, hostProcess, &remoteSection, FILE_MAP_READ, FALSE, 0)) throw std::runtime_error(std::format("DuplicateHandle(section) failed: {}", GetLastError()));
    log(std::format("Handles duplicated into host pid {}: texture {} section {}", hostPid, uintptr_t(remoteTexture), uintptr_t(remoteSection)));

    //Frames
    std::atomic<uint64_t> captured = 0, written = 0, skipped = 0, sizeMismatch = 0;
    auto pool = Direct3D11CaptureFramePool::CreateFreeThreaded(direct3dDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
    auto session = pool.CreateCaptureSession(item);
    try { session.IsBorderRequired(false); log("IsBorderRequired(false): OK"); }
    catch (const winrt::hresult_error& error) { log(std::format("IsBorderRequired(false) failed: 0x{:08X}", uint32_t(error.code()))); }

    pool.FrameArrived([&](const Direct3D11CaptureFramePool& sender, const auto&) {
      auto arrived = Spike::QpcNow100ns();
      auto frame = sender.TryGetNextFrame();
      if (!frame) return;
      auto index = ++captured;

      winrt::com_ptr<ID3D11Texture2D> texture;
      winrt::check_hresult(frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>()->GetInterface(IID_PPV_ARGS(texture.put())));
      D3D11_TEXTURE2D_DESC frameDescription;
      texture->GetDesc(&frameDescription);
      if (frameDescription.Width != description.Width || frameDescription.Height != description.Height) { sizeMismatch++; return; }

      if (mutex->AcquireSync(0, 0) != S_OK)
      {
        skipped++;
        return;
      }
      context->CopyResource(shared.get(), texture.get());
      meta->FrameIndex = index;
      meta->CaptureTime = frame.SystemRelativeTime().count();
      meta->ArrivedTime = arrived;
      meta->WriterSkipped = skipped;
      meta->Width = description.Width;
      meta->Height = description.Height;
      meta->WriteTime = Spike::QpcNow100ns();
      mutex->ReleaseSync(1);
      written++;
    });
    session.StartCapture();

    Spike::WriteAll(Spike::OutDir() / L"frames.txt", std::format("texture={}\nsection={}\nwidth={}\nheight={}\nwriter_adapter={}\n",
      uintptr_t(remoteTexture), uintptr_t(remoteSection), description.Width, description.Height, adapterName));
    log("frames.txt written, capturing");

    auto start = std::chrono::steady_clock::now();
    auto lastReport = start;
    uint64_t lastCaptured = 0;
    MSG message;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(seconds))
    {
      while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);

      if (std::chrono::steady_clock::now() - lastReport >= std::chrono::seconds(5))
      {
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - lastReport).count();
        lastReport = std::chrono::steady_clock::now();
        log(std::format("captured {} (+{:.1f} fps), written {}, skipped (reader busy) {}, size mismatch {}", captured.load(), (captured - lastCaptured) / elapsed, written.load(), skipped.load(), sizeMismatch.load()));
        lastCaptured = captured;
      }
    }

    session.Close();
    pool.Close();
    log(std::format("SUMMARY captured={} written={} skipped={} over {} s", captured.load(), written.load(), skipped.load(), seconds));
    DestroyWindow(window);
  }
  catch (const winrt::hresult_error& error)
  {
    log(std::format("ERROR 0x{:08X} {}", uint32_t(error.code()), winrt::to_string(error.message())));
  }
  catch (const std::exception& error)
  {
    log(std::format("ERROR {}", error.what()));
  }

  log("END");
  return 0;
}

