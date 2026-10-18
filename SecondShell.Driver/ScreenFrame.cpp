#include "pch.h"
#include "ScreenFrame.h"

using namespace std;

namespace SecondShell::Capture
{
  ScreenFrame::ScreenFrame(uint64_t number, Pixels pixels) :
    _number(number),
    _bounds(pixels.Bounds()),
    _pixels(move(pixels))
  {
    call_once(_isReadBack, [] {});
  }

  ScreenFrame::ScreenFrame(uint64_t number, winrt::com_ptr<ID3D11Texture2D> texture, const Rect& bounds, function<Pixels()> readBack) :
    _number(number),
    _bounds(bounds),
    _texture(move(texture)),
    _readBack(move(readBack))
  { }

  uint64_t ScreenFrame::Number() const
  {
    return _number;
  }

  const Rect& ScreenFrame::Bounds() const
  {
    return _bounds;
  }

  const Pixels& ScreenFrame::CpuPixels() const
  {
    call_once(_isReadBack, [this] { _pixels = _readBack(); });
    return _pixels;
  }

  ID3D11Texture2D* ScreenFrame::Texture() const
  {
    return _texture.get();
  }
}
