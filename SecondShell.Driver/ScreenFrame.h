#pragma once
#include "Pixels.h"

namespace SecondShell::Capture
{
  //A captured frame of the screen, kept as it was. Its pixels are read back to the CPU on the first request only; an
  //engine on the GPU can use its texture instead.
  class ScreenFrame
  {
  public:
    //A frame already on the CPU.
    ScreenFrame(uint64_t number, Pixels pixels);
    //A frame on the GPU, read back with the function when asked.
    ScreenFrame(uint64_t number, winrt::com_ptr<ID3D11Texture2D> texture, const Rect& bounds, std::function<Pixels()> readBack);

    ScreenFrame(const ScreenFrame&) = delete;
    ScreenFrame& operator=(const ScreenFrame&) = delete;

    //Counts the frames the capture delivered, so that changes can be matched to frames.
    uint64_t Number() const;
    const Rect& Bounds() const;

    //Read back on the first call, from any thread.
    const Pixels& CpuPixels() const;
    //Null for a frame only on the CPU.
    ID3D11Texture2D* Texture() const;

  private:
    uint64_t _number;
    Rect _bounds;
    winrt::com_ptr<ID3D11Texture2D> _texture;
    std::function<Pixels()> _readBack;

    mutable std::once_flag _isReadBack;
    mutable Pixels _pixels;
  };
}
