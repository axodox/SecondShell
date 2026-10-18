#include "pch.h"
#include "WinUiSwapChain.h"

using namespace Axodox::Graphics;
using namespace DirectX;
using namespace std;

namespace SecondShell::Rendering
{
  WinUiSwapChain::WinUiSwapChain(const GraphicsDevice& device, const SwapChainPanel& panel) :
    SwapChain(device, SwapChainFlags::Default),
    _panel(panel)
  {
    ReadPanel();

    auto description = CreateDescription();
    winrt::com_ptr<IDXGIFactory4> factory;
    winrt::check_hresult(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.put())));

    winrt::com_ptr<IDXGISwapChain1> swapChain;
    winrt::check_hresult(factory->CreateSwapChainForComposition(device.get(), &description, nullptr, swapChain.put()));
    winrt::check_hresult(_panel.as<ISwapChainPanelNative>()->SetSwapChain(swapChain.get()));

    //Resized by the render thread, which also draws again
    _sizeChangedRevoker = _panel.SizeChanged(winrt::auto_revoke, [this](auto&&, auto&&) { ReadPanel(); });
    _compositionScaleChangedRevoker = _panel.CompositionScaleChanged(winrt::auto_revoke, [this](auto&&, auto&&) { ReadPanel(); });

    InitializeSwapChain(swapChain);
  }

  WinUiSwapChain::~WinUiSwapChain()
  {
    _sizeChangedRevoker.revoke();
    _compositionScaleChangedRevoker.revoke();
  }

  bool WinUiSwapChain::ResizeIfChanged()
  {
    if (!_isChanged.exchange(false)) return false;

    //Also resets the transformation for the scale
    Resize();
    return true;
  }

  float WinUiSwapChain::Scale() const
  {
    lock_guard lock{ _mutex };
    return _scaleX;
  }

  XMUINT2 WinUiSwapChain::GetSize() const
  {
    lock_guard lock{ _mutex };
    return {
      max(uint32_t(_width * _scaleX), _minBufferSize),
      max(uint32_t(_height * _scaleY), _minBufferSize)
    };
  }

  DXGI_MATRIX_3X2_F WinUiSwapChain::GetTransformation() const
  {
    lock_guard lock{ _mutex };
    return { 1.f / _scaleX, 0.f, 0.f, 1.f / _scaleY, 0.f, 0.f };
  }

  void WinUiSwapChain::ReadPanel()
  {
    lock_guard lock{ _mutex };
    _width = float(_panel.ActualWidth());
    _height = float(_panel.ActualHeight());
    _scaleX = _panel.CompositionScaleX();
    _scaleY = _panel.CompositionScaleY();
    _isChanged = true;
  }
}
