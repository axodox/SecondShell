#pragma once
#include "ChangedLines.h"
#include "ITextRecognizer.h"
#include "ScreenChanges.h"
#include "ScreenTextCache.h"

namespace SecondShell::Capture
{
  //Reads the text of areas of the screen (windows, bands) with one recognizer, and keeps what each area read with the
  //fingerprint of its pixels. An area whose pixels did not change is not read again; one which changed is read again
  //in the lines which changed, if the changes since its last reading are known, else whole. All that is to be read
  //goes to the recognizer packed into as few atlases as it takes, one after the other.
  class AreaTextReader
  {
  public:
    explicit AreaTextReader(std::shared_ptr<ITextRecognizer> recognizer);

    //The words of each area, in the order of the areas. On a stop request it returns what it knew without reading.
    std::vector<std::vector<ScreenWord>> Read(const std::shared_ptr<const ScreenFrame>& frame, std::span<const Rect> areas, const ScreenChanges& changes, std::stop_token stop = {});

  private:
    //An area to read again, and the strips of it to read
    struct Plan
    {
      size_t Area;
      uint64_t Fingerprint;
      std::vector<LineStrip> Strips;
      std::vector<ScreenWord> Words;  //those it keeps, then those read
    };

    std::shared_ptr<ITextRecognizer> _recognizer;
    ScreenTextCache _texts;

    Plan PlanReading(size_t index, const Rect& area, uint64_t fingerprint, const std::optional<AreaText>& known, const ScreenChanges& changes) const;
    //The words of each rectangle, on the screen; empty if stopped.
    std::optional<std::vector<std::vector<ScreenWord>>> Recognize(const std::shared_ptr<const ScreenFrame>& frame, std::span<const Rect> rectangles, std::stop_token stop) const;
  };
}
