#pragma once
#include "ITextRecognizer.h"
#include "ScreenWord.h"

namespace SecondShell::Tests
{
  //Reads the words it is given, on the screen, which are whole in a cell of the atlas; remembers the atlases.
  class FakeTextRecognizer : public Capture::ITextRecognizer
  {
  public:
    std::vector<Capture::ScreenWord> Words;
    uint32_t MaxSide = 10000;
    mutable std::vector<Capture::AtlasLayout> Atlases;

    uint32_t MaxAtlasSide() const override
    {
      return MaxSide;
    }

    std::future<std::vector<Capture::AtlasWord>> RecognizeAsync(std::shared_ptr<const Capture::TextAtlas> atlas) const override
    {
      auto& layout = atlas->Layout();
      auto pixels = atlas->CpuPixels();
      Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(pixels.Width == layout.Width && pixels.Height == layout.Height, L"the atlas's pixels");
      Atlases.push_back(layout);

      std::vector<Capture::AtlasWord> found;
      for (auto& cell : layout.Cells)
      {
        for (auto& word : Words)
        {
          if (word.Box.Intersection(cell.Source) != word.Box) continue;
          found.push_back({ word.Text, { word.Box.X - cell.Source.X + cell.X, word.Box.Y - cell.Source.Y + cell.Y, word.Box.Width, word.Box.Height } });
        }
      }

      std::promise<std::vector<Capture::AtlasWord>> result;
      result.set_value(move(found));
      return result.get_future();
    }

    size_t Calls() const
    {
      return Atlases.size();
    }

    //The rectangles of the screen read, in all atlases
    std::vector<Capture::Rect> Read() const
    {
      std::vector<Capture::Rect> read;
      for (auto& atlas : Atlases)
      {
        for (auto& cell : atlas.Cells) read.push_back(cell.Source);
      }
      return read;
    }
  };

  inline std::shared_ptr<const Capture::ScreenFrame> Frame(uint64_t number, uint32_t width, uint32_t height, uint8_t value = 255)
  {
    return std::make_shared<Capture::ScreenFrame>(number, Capture::Pixels{ width, height, std::vector<uint8_t>(size_t(width) * height * 4, value) });
  }

  //A copy of the frame's pixels with one pixel changed
  inline std::shared_ptr<const Capture::ScreenFrame> Changed(const Capture::ScreenFrame& frame, uint32_t x, uint32_t y)
  {
    auto pixels = frame.CpuPixels();
    pixels.Bgra[(size_t(y) * pixels.Width + x) * 4] ^= 0xFF;
    return std::make_shared<Capture::ScreenFrame>(frame.Number() + 1, std::move(pixels));
  }
}
