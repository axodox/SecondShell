#include "pch.h"
#include "TextAtlas.h"

using namespace std;

namespace SecondShell::Capture
{
  TextAtlas::TextAtlas(shared_ptr<const ScreenFrame> frame, AtlasLayout layout) :
    _frame(move(frame)),
    _layout(move(layout))
  { }

  const ScreenFrame& TextAtlas::Frame() const
  {
    return *_frame;
  }

  const AtlasLayout& TextAtlas::Layout() const
  {
    return _layout;
  }

  Pixels TextAtlas::CpuPixels() const
  {
    auto& screen = _frame->CpuPixels();
    Pixels atlas{ _layout.Width, _layout.Height, vector<uint8_t>(size_t(_layout.Width) * _layout.Height * 4, Background) };

    for (auto& cell : _layout.Cells)
    {
      auto rowSize = size_t(cell.Source.Width) * 4;
      for (uint32_t row = 0; row < cell.Source.Height; row++)
      {
        auto source = screen.Bgra.data() + (size_t(cell.Source.Y + row) * screen.Width + cell.Source.X) * 4;
        auto target = atlas.Bgra.data() + (size_t(cell.Y + row) * atlas.Width + cell.X) * 4;
        memcpy(target, source, rowSize);
      }
    }
    return atlas;
  }
}
