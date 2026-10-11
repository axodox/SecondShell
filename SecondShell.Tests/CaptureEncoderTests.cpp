#include "pch.h"
#include "CaptureEncoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Capture;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(CaptureEncoderTests)
  {
    HRESULT _comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    static Pixels Gradient(uint32_t width, uint32_t height)
    {
      Pixels pixels{ width, height, vector<uint8_t>(size_t(width) * height * 4) };
      for (size_t index = 0; index < pixels.Bgra.size(); index++) pixels.Bgra[index] = uint8_t(index);
      return pixels;
    }

  public:
    ~CaptureEncoderTests()
    {
      if (SUCCEEDED(_comResult)) CoUninitialize();
    }

    TEST_METHOD(ImagesAreEncodedAtFullSize)
    {
      auto image = CaptureEncoder().Encode(Gradient(40, 20), nullopt, ImageFormat::Png);
      Assert::AreEqual(40u, image.Width);
      Assert::AreEqual(20u, image.Height);
      Assert::AreEqual(1.0, image.Scale);
      Assert::IsTrue(image.Data.size() > 8 && image.Data[1] == 'P' && image.Data[2] == 'N' && image.Data[3] == 'G', L"a PNG file");
    }

    TEST_METHOD(ImagesAreScaledDownToFitNeverUp)
    {
      auto image = CaptureEncoder().Encode(Gradient(40, 20), Size{ 10, 10 }, ImageFormat::Jpeg);
      Assert::AreEqual(10u, image.Width);
      Assert::AreEqual(5u, image.Height, L"the aspect ratio is kept");
      Assert::AreEqual(0.25, image.Scale);
      Assert::IsTrue(image.Data.size() > 2 && image.Data[0] == 0xFF && image.Data[1] == 0xD8, L"a JPEG file");

      image = CaptureEncoder().Encode(Gradient(40, 20), Size{ 100, 100 }, ImageFormat::Png);
      Assert::AreEqual(40u, image.Width, L"never scaled up");
    }
  };
}
