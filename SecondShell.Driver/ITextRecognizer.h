#pragma once
#include "TextAtlas.h"

namespace SecondShell::Capture
{
  //A word an engine found in an atlas.
  struct AtlasWord
  {
    std::string Text;
    Rect Box;  //in the atlas; its layout maps it to the screen
  };

  //An OCR engine, or a pipeline of them (e.g. a text detector feeding another recognizer). Its device, the CPU or a
  //GPU, is chosen when it is created; recognition is asynchronous, and its input is a frame's regions rather than
  //pixels, so an engine on the GPU can read the captured texture without a copy to the CPU. The text waits use one
  //per tier, so engines can be swapped or mixed without changing the waits.
  class ITextRecognizer
  {
  public:
    virtual ~ITextRecognizer() = default;

    //The largest atlas side it reads, in screen pixels: its largest image at the scale it reads them.
    virtual uint32_t MaxAtlasSide() const = 0;

    virtual std::future<std::vector<AtlasWord>> RecognizeAsync(std::shared_ptr<const TextAtlas> atlas) const = 0;
  };
}
