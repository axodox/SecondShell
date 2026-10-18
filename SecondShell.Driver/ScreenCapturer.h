#pragma once
#include "FrameRateMeter.h"

namespace SecondShell::Capture
{
  struct Rect
  {
    int32_t X = 0;
    int32_t Y = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
  };

  //Pixels of a region, 32-bit BGRA, rows without padding.
  struct Pixels
  {
    uint32_t Width = 0;
    uint32_t Height = 0;
    std::vector<uint8_t> Bgra;
  };

  //A new frame, for the FrameArrived handlers: valid during the call only, on the capture's device context.
  struct CapturedFrame
  {
    ID3D11DeviceContext* Context;
    ID3D11Texture2D* Texture;
    uint32_t Width;
    uint32_t Height;
  };

  //Captures the session's desktop (its primary and only monitor) with Windows.Graphics.Capture and keeps the newest
  //frame. Frames arrive only when the screen changes, so the newest frame is the screen as it is now.
  class ScreenCapturer
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ScreenCapturer" };
    Axodox::Infrastructure::event_owner _events;

  public:
    ScreenCapturer();
    ~ScreenCapturer();

    ScreenCapturer(const ScreenCapturer&) = delete;
    ScreenCapturer& operator=(const ScreenCapturer&) = delete;

    void Start();

    //A region of the newest frame; waits for the first frame. Throws if the region is not inside the screen.
    Pixels Read(const Rect& region, std::chrono::milliseconds timeout);
    //The screen as the newest frame shows it, so captures and their regions agree, also while a resolution change
    //reaches the frames.
    Rect Screen() const;

    //The capture's device; its context is the capturer's own.
    ID3D11Device* Device() const;

    //The adapter of the capture's device, as a LUID packed into 64 bits.
    uint64_t AdapterLuid() const;

    //Raised on the frame pool's thread, with the capturer locked: handlers must not call into it.
    Axodox::Infrastructure::event_publisher<ScreenCapturer*, const CapturedFrame&> FrameArrived;

  private:
    winrt::com_ptr<ID3D11Device> _device;
    winrt::com_ptr<ID3D11DeviceContext> _context;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice _captureDevice{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem _item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool _framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession _session{ nullptr };
    winrt::Windows::Graphics::SizeInt32 _poolSize{};  //after Start, used on the frame pool's thread only
    Graphics::FrameRateMeter _frameRate{ std::chrono::seconds(10) };  //the frame pool's thread only

    //Guards the context and the newest frame, which the frame pool's thread replaces
    mutable std::mutex _mutex;
    std::condition_variable _frameArrived;
    winrt::com_ptr<ID3D11Texture2D> _frame;

    void OnFrameArrived(const winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool& sender, const winrt::Windows::Foundation::IInspectable& args);
  };
}
