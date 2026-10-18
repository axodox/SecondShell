#pragma once

namespace SecondShell::Rendering
{
  //Axodox's swap chain on a WinUI 3 SwapChainPanel (Axodox's XamlSwapChain is for UWP XAML). Create it on the UI thread;
  //the render thread presents, and resizes it when the panel has changed.
  class WinUiSwapChain : public Axodox::Graphics::SwapChain
  {
    using SwapChainPanel = winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel;

  public:
    WinUiSwapChain(const Axodox::Graphics::GraphicsDevice& device, const SwapChainPanel& panel);
    ~WinUiSwapChain();

    //On the render thread, with no back buffer in use: true if the panel changed and the buffers were resized, then
    //what they showed has to be drawn again.
    bool ResizeIfChanged();

    //Physical pixels per device independent pixel.
    float Scale() const;

  protected:
    virtual DirectX::XMUINT2 GetSize() const override;
    virtual DXGI_MATRIX_3X2_F GetTransformation() const override;

  private:
    SwapChainPanel _panel;
    SwapChainPanel::SizeChanged_revoker _sizeChangedRevoker;
    SwapChainPanel::CompositionScaleChanged_revoker _compositionScaleChangedRevoker;

    //Updated on the UI thread, read on the render thread
    mutable std::mutex _mutex;
    float _width = 0;
    float _height = 0;
    float _scaleX = 1;
    float _scaleY = 1;
    std::atomic_bool _isChanged = false;

    void ReadPanel();
  };
}
