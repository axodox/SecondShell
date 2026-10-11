#include "pch.h"
#include "ScreenCapturer.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace SecondShell::Capture
{
  ScreenCapturer::ScreenCapturer()
  {
    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, _device.put(), nullptr, _context.put()));

    winrt::com_ptr<::IInspectable> device;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(_device.as<IDXGIDevice>().get(), device.put()));
    _captureDevice = device.as<IDirect3DDevice>();
  }

  ScreenCapturer::~ScreenCapturer()
  {
    if (_session) _session.Close();
    if (_framePool) _framePool.Close();
  }

  void ScreenCapturer::Start()
  {
    auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::check_hresult(interop->CreateForMonitor(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), winrt::guid_of<GraphicsCaptureItem>(), winrt::put_abi(_item)));

    _poolSize = _item.Size();
    _framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(_captureDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, _poolSize);
    _framePool.FrameArrived({ this, &ScreenCapturer::OnFrameArrived });

    _session = _framePool.CreateCaptureSession(_item);
    _session.IsBorderRequired(false);
    _session.IsCursorCaptureEnabled(true);
    _session.StartCapture();

    _logger.log(log_severity::information, "Capturing {}x{}.", _poolSize.Width, _poolSize.Height);
  }

  Pixels ScreenCapturer::Read(const Rect& region, milliseconds timeout)
  {
    unique_lock lock{ _mutex };
    if (!_frameArrived.wait_for(lock, timeout, [this] { return _frame != nullptr; })) throw runtime_error("The screen could not be captured.");

    D3D11_TEXTURE2D_DESC description;
    _frame->GetDesc(&description);
    if (region.X < 0 || region.Y < 0 || region.Width == 0 || region.Height == 0 || region.X + region.Width > description.Width || region.Y + region.Height > description.Height)
    {
      throw invalid_argument(format("The region is not inside the screen, which is {}x{} pixels.", description.Width, description.Height));
    }

    //Copy the region to a texture the CPU can read
    description.Width = region.Width;
    description.Height = region.Height;
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;

    winrt::com_ptr<ID3D11Texture2D> staging;
    winrt::check_hresult(_device->CreateTexture2D(&description, nullptr, staging.put()));

    D3D11_BOX box{ UINT(region.X), UINT(region.Y), 0, UINT(region.X) + region.Width, UINT(region.Y) + region.Height, 1 };
    _context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, _frame.get(), 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped;
    winrt::check_hresult(_context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));

    Pixels result{ region.Width, region.Height, vector<uint8_t>(size_t(region.Width) * region.Height * 4) };
    auto rowSize = size_t(region.Width) * 4;
    for (uint32_t row = 0; row < region.Height; row++)
    {
      memcpy(result.Bgra.data() + row * rowSize, static_cast<uint8_t*>(mapped.pData) + row * mapped.RowPitch, rowSize);
    }

    _context->Unmap(staging.get(), 0);
    return result;
  }

  Rect ScreenCapturer::Screen() const
  {
    return { 0, 0, uint32_t(GetSystemMetrics(SM_CXSCREEN)), uint32_t(GetSystemMetrics(SM_CYSCREEN)) };
  }

  void ScreenCapturer::OnFrameArrived(const Direct3D11CaptureFramePool& sender, const winrt::Windows::Foundation::IInspectable& /*args*/)
  {
    auto frame = sender.TryGetNextFrame();
    if (!frame) return;

    //A resolution change: frames of the new size arrive after the pool is recreated
    auto size = frame.ContentSize();
    if (size != _poolSize)
    {
      _poolSize = size;
      sender.Recreate(_captureDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
    }

    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>()->GetInterface(IID_PPV_ARGS(texture.put())));

    D3D11_TEXTURE2D_DESC description;
    texture->GetDesc(&description);
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = 0;
    description.CPUAccessFlags = 0;
    description.MiscFlags = 0;

    //Kept in a texture of our own, as the frame goes back to the pool
    {
      lock_guard lock{ _mutex };
      D3D11_TEXTURE2D_DESC current{};
      if (_frame) _frame->GetDesc(&current);
      if (!_frame || current.Width != description.Width || current.Height != description.Height)
      {
        _frame = nullptr;
        winrt::check_hresult(_device->CreateTexture2D(&description, nullptr, _frame.put()));
      }

      _context->CopyResource(_frame.get(), texture.get());
    }
    _frameArrived.notify_all();
  }
}
