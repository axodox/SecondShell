#include "pch.h"
#include "SessionRenderer.h"

using namespace Axodox::Graphics;
using namespace Axodox::Infrastructure;
using namespace SecondShell::Agent;
using namespace SecondShell::Graphics;
using namespace std;
using namespace std::chrono;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace SecondShell::Rendering
{
  const D2D1_COLOR_F _background{ 0.071f, 0.071f, 0.082f, 1.f };  //#121215, as the session area around the view

  //Who is in control: the agent violet (#8B7CFF), the user amber (#FFB547)
  const D2D1_COLOR_F _agentFrameColor{ 0.545f, 0.486f, 1.f, 0.8f };
  const D2D1_COLOR_F _userFrameColor{ 1.f, 0.71f, 0.278f, 1.f };
  const float _frameWidth = 2.f;

  namespace
  {
    LUID ToLuid(uint64_t value)
    {
      return { DWORD(value & 0xFFFFFFFF), LONG(value >> 32) };
    }
  }

  SessionRenderer::SessionRenderer(dependency_container* container) :
    _frames(container->resolve<FrameReceiver>()),
    _overlay(container->resolve<OverlayRenderer>()),
    _agentControl(container->resolve<AgentControl>()),
    _modeChangedSubscription(_agentControl->ModeChanged({ this, &SessionRenderer::OnModeChanged }))
  { }

  SessionRenderer::~SessionRenderer()
  {
    Detach();
  }

  void SessionRenderer::Attach(const SwapChainPanel& panel, HWND window)
  {
    Detach();

    _panel = panel;
    _clock = make_unique<DisplayClock>(window);
    _uiQueue = panel.DispatcherQueue();
    _isRunning = true;
    _thread = thread([this] { Run(); });
  }

  void SessionRenderer::Detach()
  {
    if (!_thread.joinable()) return;

    _isRunning = false;
    _thread.join();

    //On the UI thread, as the swap chain was made there
    lock_guard lock{ _mutex };
    _reader.reset();
    _swapChain.reset();
    _drawing.reset();
    _device.reset();
    _adapterLuid.reset();
    _panel = nullptr;
  }

  optional<ViewTransform> SessionRenderer::Transform() const
  {
    lock_guard lock{ _transformMutex };
    return _transform;
  }

  void SessionRenderer::Run()
  {
    while (_isRunning)
    {
      {
        lock_guard lock{ _mutex };
        if (_frames->Generation() != _generation)
        {
          if (!_isRebuildPending) _isRebuildPending = _uiQueue.TryEnqueue([this] { Rebuild(); });
        }
        else if (_swapChain)
        {
          try
          {
            DrawIfChanged();
          }
          catch (const winrt::hresult_error& error)
          {
            //E.g. a lost device: everything is made again
            _logger.log(log_severity::error, "Drawing the session failed: {}", winrt::to_string(error.message()));
            _reader.reset();
            _adapterLuid.reset();
            _isRedrawPending = false;
            _generation = ~_frames->Generation();
          }
        }
      }

      //A new frame is drawn at the next refresh of the host's monitor, so the view keeps up with any refresh rate
      _clock->Wait();
    }
  }

  void SessionRenderer::DrawIfChanged()
  {
    auto isResized = _swapChain->ResizeIfChanged();
    if (!_reader)
    {
      if (isResized) Draw(nullptr);
      return;
    }

    //A new frame, or the newest again when the view has changed: a new size, or the overlay moving on
    _isRedrawPending |= isResized || _overlay->IsAnimating();
    auto frame = _reader->TryLockNewer();
    if (frame)
    {
      if (auto rate = _frameRate.Tick()) _logger.log(log_severity::information, "Showing new frames at {:.1f} fps.", *rate);
    }
    else if (_isRedrawPending) frame = _reader->LockNewest();

    //One the driver holds is drawn at the next check
    if (!frame) return;

    Draw(frame.Texture());
    _isRedrawPending = false;
  }

  void SessionRenderer::Rebuild()
  {
    lock_guard lock{ _mutex };
    _isRebuildPending = false;
    if (!_panel) return;

    try
    {
      //The frames' textures open only on a device of their adapter (Findings.md 2.13)
      auto adapterLuid = _frames->AdapterLuid();
      if (adapterLuid && adapterLuid != _adapterLuid)
      {
        _reader.reset();
        _swapChain.reset();
        _drawing.reset();
        _device = make_unique<GraphicsDevice>(GraphicsDeviceFlags::None, ToLuid(*adapterLuid));
        _drawing = make_unique<DrawingController>(*_device);
        _swapChain = make_unique<WinUiSwapChain>(*_device, _panel);
        _adapterLuid = adapterLuid;
      }

      _reader = _device ? _frames->CreateReader(_device->get(), _generation) : nullptr;
      if (!_reader)
      {
        _generation = _frames->Generation();
        if (_swapChain) Draw(nullptr);
      }
    }
    catch (const winrt::hresult_error& error)
    {
      _logger.log(log_severity::error, "The session view could not be set up: {}", winrt::to_string(error.message()));
      _reader.reset();
      _generation = _frames->Generation();
    }
    catch (const exception& exception)
    {
      _logger.log(log_severity::error, "The session view could not be set up: {}", exception.what());
      _reader.reset();
      _generation = _frames->Generation();
    }
  }

  void SessionRenderer::Draw(ID3D11Texture2D* frame)
  {
    winrt::com_ptr<ID3D11Texture2D> backBuffer;
    backBuffer.copy_from(static_cast<Texture2D*>(_swapChain->BackBuffer())->get());

    //Released before the present, as the swap chain may resize after it
    {
      auto target = _drawing->CreateBitmap(backBuffer);
      auto context = _drawing->DrawingContext();
      context->SetTarget(target.get());
      context->BeginDraw();
      context->Clear(_background);

      if (frame)
      {
        winrt::com_ptr<IDXGISurface> surface;
        winrt::check_hresult(frame->QueryInterface(IID_PPV_ARGS(surface.put())));

        winrt::com_ptr<ID2D1Bitmap1> bitmap;
        auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        winrt::check_hresult(context->CreateBitmapFromDxgiSurface(surface.get(), properties, bitmap.put()));

        auto transform = ViewTransform::Letterbox(target->GetPixelSize(), _reader->Width(), _reader->Height(), _swapChain->Scale());
        context->DrawBitmap(bitmap.get(), transform.Session, 1.f, D2D1_INTERPOLATION_MODE_LINEAR);
        DrawFrame(context, transform, target->GetPixelSize());
        _overlay->Draw(context, transform, steady_clock::now());

        lock_guard lock{ _transformMutex };
        _transform = transform;
      }

      winrt::check_hresult(context->EndDraw());
      context->SetTarget(nullptr);
    }

    _swapChain->Present();
  }

  void SessionRenderer::DrawFrame(ID2D1DeviceContext* context, const ViewTransform& transform, D2D1_SIZE_U view) const
  {
    auto mode = _agentControl->Mode();
    if (mode == ControlMode::Stopped) return;

    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(context->CreateSolidColorBrush(mode == ControlMode::User ? _userFrameColor : _agentFrameColor, brush.put()));

    //Around the session, and over its edge where the session meets the view's
    auto width = transform.Ui(_frameWidth);
    auto half = width / 2;
    auto& session = transform.Session;
    D2D1_RECT_F frame{
      max(session.left - half, half),
      max(session.top - half, half),
      min(session.right + half, view.width - half),
      min(session.bottom + half, view.height - half)
    };
    context->DrawRectangle(frame, brush.get(), width);
  }

  void SessionRenderer::OnModeChanged(AgentControl* /*sender*/, ControlMode /*mode*/)
  {
    lock_guard lock{ _mutex };
    _isRedrawPending = true;
  }
}
