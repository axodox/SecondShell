#include "pch.h"
#include "FrameReader.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")

std::vector<AdapterInfo> EnumerateAdapters()
{
  std::vector<AdapterInfo> result;
  winrt::com_ptr<IDXGIFactory1> factory;
  winrt::check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(factory.put())));
  winrt::com_ptr<IDXGIAdapter1> adapter;
  for (UINT i = 0; factory->EnumAdapters1(i, adapter.put()) != DXGI_ERROR_NOT_FOUND; i++)
  {
    DXGI_ADAPTER_DESC1 description;
    adapter->GetDesc1(&description);
    result.push_back({ description.AdapterLuid, Spike::Narrow(description.Description) });
    adapter = nullptr;
  }
  return result;
}

FrameReader::FrameReader(Spike::Logger& log, const AdapterInfo& adapter) :
  _log(log),
  _adapter(adapter)
{
  winrt::com_ptr<IDXGIFactory4> factory;
  winrt::check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(factory.put())));
  winrt::com_ptr<IDXGIAdapter1> dxgiAdapter;
  winrt::check_hresult(factory->EnumAdapterByLuid(adapter.Luid, IID_PPV_ARGS(dxgiAdapter.put())));

  winrt::com_ptr<ID3D11Device> device;
  winrt::check_hresult(D3D11CreateDevice(dxgiAdapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, _context.put()));
  _device = device.as<ID3D11Device1>();
}

void FrameReader::Run(const std::filesystem::path& framesFile, std::chrono::seconds duration, const std::function<bool()>& stopRequested)
{
  auto values = Spike::ReadKeyValues(framesFile);
  auto textureHandle = HANDLE(uintptr_t(std::stoull(values["texture"])));
  auto sectionHandle = HANDLE(uintptr_t(std::stoull(values["section"])));
  auto width = uint32_t(std::stoul(values["width"]));
  auto height = uint32_t(std::stoul(values["height"]));
  _log(std::format("FRAMES: writer reports texture handle {} section handle {} {}x{}, writer adapter '{}'", values["texture"], values["section"], width, height, values["writer_adapter"]));

  //Open the duplicated handles
  winrt::com_ptr<ID3D11Texture2D> shared;
  auto result = _device->OpenSharedResource1(textureHandle, IID_PPV_ARGS(shared.put()));
  if (FAILED(result))
  {
    _log(std::format("FRAMES: OpenSharedResource1 FAILED 0x{:08X} on reader adapter '{}'", uint32_t(result), _adapter.Name));
    return;
  }
  D3D11_TEXTURE2D_DESC description;
  shared->GetDesc(&description);
  _log(std::format("FRAMES: OpenSharedResource1 OK: {}x{} format {} misc 0x{:X}", description.Width, description.Height, int(description.Format), description.MiscFlags));

  auto mutex = shared.as<IDXGIKeyedMutex>();
  auto meta = static_cast<const Spike::SharedFrameMeta*>(MapViewOfFile(sectionHandle, FILE_MAP_READ, 0, 0, sizeof(Spike::SharedFrameMeta)));
  if (!meta)
  {
    _log(std::format("FRAMES: MapViewOfFile FAILED {}", GetLastError()));
    return;
  }

  auto stagingDescription = description;
  stagingDescription.Usage = D3D11_USAGE_STAGING;
  stagingDescription.BindFlags = 0;
  stagingDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  stagingDescription.MiscFlags = 0;
  winrt::com_ptr<ID3D11Texture2D> staging;
  winrt::check_hresult(_device->CreateTexture2D(&stagingDescription, nullptr, staging.put()));

  auto localDescription = description;
  localDescription.MiscFlags = 0;
  winrt::com_ptr<ID3D11Texture2D> local;
  winrt::check_hresult(_device->CreateTexture2D(&localDescription, nullptr, local.put()));

  //Read frames
  std::vector<double> latencies, arrivals, handoffs, lockTimes;
  uint64_t frames = 0, timeouts = 0, gaps = 0, lastIndex = 0, firstIndex = 0, writerSkipped = 0;
  bool savedFirst = false;
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < duration && !stopRequested())
  {
    result = mutex->AcquireSync(1, 500);
    if (result == static_cast<HRESULT>(WAIT_TIMEOUT))
    {
      timeouts++;
      continue;
    }
    if (FAILED(result) || result == static_cast<HRESULT>(WAIT_ABANDONED))
    {
      _log(std::format("FRAMES: AcquireSync returned 0x{:08X}", uint32_t(result)));
      break;
    }

    auto acquired = Spike::QpcNow100ns();
    auto frame = *meta;
    _context->CopyResource(local.get(), shared.get());

    bool save = !savedFirst && std::chrono::steady_clock::now() - start > std::chrono::seconds(3);
    if (save) _context->CopyResource(staging.get(), shared.get());
    _context->Flush();
    mutex->ReleaseSync(0);
    lockTimes.push_back((Spike::QpcNow100ns() - acquired) / 10'000.0);

    frames++;
    if (firstIndex == 0) firstIndex = frame.FrameIndex;
    if (lastIndex && frame.FrameIndex > lastIndex + 1) gaps += frame.FrameIndex - lastIndex - 1;
    lastIndex = frame.FrameIndex;
    writerSkipped = frame.WriterSkipped;
    latencies.push_back((acquired - frame.CaptureTime) / 10'000.0);
    arrivals.push_back((acquired - frame.ArrivedTime) / 10'000.0);
    handoffs.push_back((acquired - frame.WriteTime) / 10'000.0);

    if (save)
    {
      SavePng(staging.get(), description.Width, description.Height, Spike::OutDir() / L"frame_host.png");
      savedFirst = true;
      _log(std::format("FRAMES: saved frame {} as frame_host.png", frame.FrameIndex));
    }
  }
  auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

  auto stats = [](std::vector<double> values) {
    if (values.empty()) return std::string("n/a");
    std::sort(values.begin(), values.end());
    double sum = 0;
    for (auto value : values) sum += value;
    return std::format("min {:.2f} / avg {:.2f} / p50 {:.2f} / p95 {:.2f} / max {:.2f} ms", values.front(), sum / values.size(), values[values.size() / 2], values[values.size() * 95 / 100], values.back());
  };

  _log(std::format("FRAMES RESULT: {} frames in {:.1f} s ({:.1f} fps), writer frames {}..{}, missed by reader {}, writer skipped (reader busy) {}, acquire timeouts (500 ms) {}",
    frames, elapsed, frames / elapsed, firstIndex, lastIndex, gaps, writerSkipped, timeouts));
  _log("FRAMES RESULT: SystemRelativeTime-to-acquire " + stats(latencies));
  _log("FRAMES RESULT: FrameArrived-to-acquire latency " + stats(arrivals));
  _log("FRAMES RESULT: release-to-acquire handoff " + stats(handoffs));
  _log("FRAMES RESULT: reader lock held " + stats(lockTimes));

  UnmapViewOfFile(meta);
  CloseHandle(textureHandle);
  CloseHandle(sectionHandle);
}

void FrameReader::SavePng(ID3D11Texture2D* staging, uint32_t width, uint32_t height, const std::filesystem::path& path)
{
  D3D11_MAPPED_SUBRESOURCE mapped;
  winrt::check_hresult(_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));

  winrt::com_ptr<IWICImagingFactory> factory;
  winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
  winrt::com_ptr<IWICStream> stream;
  winrt::check_hresult(factory->CreateStream(stream.put()));
  winrt::check_hresult(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
  winrt::com_ptr<IWICBitmapEncoder> encoder;
  winrt::check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
  winrt::check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));
  winrt::com_ptr<IWICBitmapFrameEncode> frame;
  winrt::check_hresult(encoder->CreateNewFrame(frame.put(), nullptr));
  winrt::check_hresult(frame->Initialize(nullptr));
  winrt::check_hresult(frame->SetSize(width, height));
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  winrt::check_hresult(frame->SetPixelFormat(&format));
  winrt::check_hresult(frame->WritePixels(height, mapped.RowPitch, mapped.RowPitch * height, static_cast<BYTE*>(mapped.pData)));
  winrt::check_hresult(frame->Commit());
  winrt::check_hresult(encoder->Commit());

  _context->Unmap(staging, 0);
}

