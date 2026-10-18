#pragma once
#include "AtlasLayout.h"
#include "ScreenFrame.h"

namespace SecondShell::Capture
{
  //What a recognizer reads: rectangles of a captured frame packed into one image, with gutters of flat background
  //between them, so that words of neighbouring cells do not join. An engine on the CPU asks for the atlas's pixels,
  //composed from the frame's pixels only then; one on the GPU can compose it from the frame's texture by the layout.
  class TextAtlas
  {
  public:
    static constexpr uint8_t Background = 255;

    TextAtlas(std::shared_ptr<const ScreenFrame> frame, AtlasLayout layout);

    const ScreenFrame& Frame() const;
    const AtlasLayout& Layout() const;

    //Composed at each call.
    Pixels CpuPixels() const;

  private:
    std::shared_ptr<const ScreenFrame> _frame;
    AtlasLayout _layout;
  };
}
