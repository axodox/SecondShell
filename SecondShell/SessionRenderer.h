#pragma once
#include "AgentControl.h"
#include "DisplayClock.h"
#include "FrameReceiver.h"
#include "FrameRateMeter.h"
#include "OverlayRenderer.h"
#include "WinUiSwapChain.h"

namespace SecondShell::Rendering
{
  //Draws the session into its panel: the newest frame, letterboxed, its frame in the colour of who is in control, and the
  //overlay, on a render thread of its own. The graphics objects
  //are made on the UI thread, which the swap chain needs, on the adapter the driver's frames are on.
  class SessionRenderer
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SessionRenderer" };

  public:
    explicit SessionRenderer(Axodox::Infrastructure::dependency_container* container);
    ~SessionRenderer();

    //On the UI thread; the window is the panel's, whose monitor sets the pace.
    void Attach(const winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel& panel, HWND window);
    void Detach();

    //Where the session was last drawn; empty before the first frame.
    std::optional<ViewTransform> Transform() const;

    //New frames per second lately; empty while the session's screen does not change.
    std::optional<float> FrameRate() const;

  private:
    std::shared_ptr<FrameReceiver> _frames;
    std::shared_ptr<OverlayRenderer> _overlay;
    std::shared_ptr<Agent::AgentControl> _agentControl;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel _panel{ nullptr };
    winrt::Microsoft::UI::Dispatching::DispatcherQueue _uiQueue{ nullptr };
    std::atomic_bool _isRunning = false;
    std::unique_ptr<DisplayClock> _clock;
    std::thread _thread;

    //Guards what follows, which the UI thread replaces and the render thread draws with
    std::mutex _mutex;
    std::unique_ptr<Axodox::Graphics::GraphicsDevice> _device;
    std::unique_ptr<Axodox::Graphics::DrawingController> _drawing;
    std::unique_ptr<WinUiSwapChain> _swapChain;
    std::unique_ptr<Graphics::SharedFrameReader> _reader;
    std::optional<uint64_t> _adapterLuid;
    uint64_t _generation = 0;
    bool _isRebuildPending = false;
    bool _isRedrawPending = false;
    Graphics::FrameRateMeter _frameRateMeter{ std::chrono::seconds(2) };
    std::atomic<float> _frameRate = 0;
    std::atomic<std::chrono::steady_clock::time_point> _frameRateTime{};

    mutable std::mutex _transformMutex;
    std::optional<ViewTransform> _transform;

    void Run();

    //With the lock held, on the render thread.
    void DrawIfChanged();

    //On the UI thread: the graphics objects for the current frames.
    void Rebuild();

    //With the lock held; null for no frame, which clears the view.
    void Draw(ID3D11Texture2D* frame);
    void DrawFrame(ID2D1DeviceContext* context, const ViewTransform& transform, D2D1_SIZE_U view) const;

    void OnModeChanged(Agent::AgentControl* sender, Agent::ControlMode mode);
  };
}
