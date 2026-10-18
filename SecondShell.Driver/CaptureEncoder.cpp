#include "pch.h"
#include "CaptureEncoder.h"

using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Capture
{
  CaptureEncoder::CaptureEncoder() :
    _factory(winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory))
  { }

  CapturedImage CaptureEncoder::Encode(const Pixels& pixels, const optional<Size>& maxResolution, ImageFormat format) const
  {
    CapturedImage result{ .Width = pixels.Width, .Height = pixels.Height, .Scale = 1.0, .Format = format };

    winrt::com_ptr<IWICBitmap> bitmap;
    winrt::check_hresult(_factory->CreateBitmapFromMemory(pixels.Width, pixels.Height, GUID_WICPixelFormat32bppBGRA, pixels.Width * 4, UINT(pixels.Bgra.size()), const_cast<BYTE*>(pixels.Bgra.data()), bitmap.put()));
    winrt::com_ptr<IWICBitmapSource> source = bitmap;

    //Scale down to fit
    if (maxResolution)
    {
      result.Scale = min({ 1.0, double(maxResolution->Width) / pixels.Width, double(maxResolution->Height) / pixels.Height });
      if (result.Scale < 1.0)
      {
        result.Width = max(1u, uint32_t(lround(pixels.Width * result.Scale)));
        result.Height = max(1u, uint32_t(lround(pixels.Height * result.Scale)));

        winrt::com_ptr<IWICBitmapScaler> scaler;
        winrt::check_hresult(_factory->CreateBitmapScaler(scaler.put()));
        winrt::check_hresult(scaler->Initialize(source.get(), result.Width, result.Height, WICBitmapInterpolationModeHighQualityCubic));
        source = scaler;
      }
    }

    //Without the alpha channel, which the desktop does not use
    winrt::com_ptr<IWICFormatConverter> converter;
    winrt::check_hresult(_factory->CreateFormatConverter(converter.put()));
    winrt::check_hresult(converter->Initialize(source.get(), GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));

    //Encode
    winrt::com_ptr<IStream> stream;
    stream.attach(SHCreateMemStream(nullptr, 0));
    if (!stream) throw bad_alloc();

    winrt::com_ptr<IWICBitmapEncoder> encoder;
    winrt::check_hresult(_factory->CreateEncoder(format == ImageFormat::Jpeg ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng, nullptr, encoder.put()));
    winrt::check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    winrt::com_ptr<IWICBitmapFrameEncode> frame;
    winrt::com_ptr<IPropertyBag2> properties;
    winrt::check_hresult(encoder->CreateNewFrame(frame.put(), properties.put()));
    winrt::check_hresult(frame->Initialize(properties.get()));
    winrt::check_hresult(frame->SetSize(result.Width, result.Height));

    auto pixelFormat = GUID_WICPixelFormat24bppBGR;
    winrt::check_hresult(frame->SetPixelFormat(&pixelFormat));
    winrt::check_hresult(frame->WriteSource(converter.get(), nullptr));
    winrt::check_hresult(frame->Commit());
    winrt::check_hresult(encoder->Commit());

    //Read the bytes back
    ULARGE_INTEGER size{};
    winrt::check_hresult(IStream_Size(stream.get(), &size));
    winrt::check_hresult(IStream_Reset(stream.get()));

    result.Data.resize(size_t(size.QuadPart));
    winrt::check_hresult(IStream_Read(stream.get(), result.Data.data(), ULONG(result.Data.size())));
    return result;
  }

  Pixels CaptureEncoder::Decode(span<const uint8_t> file) const
  {
    winrt::com_ptr<IStream> stream;
    stream.attach(SHCreateMemStream(file.data(), UINT(file.size())));
    if (!stream) throw bad_alloc();

    winrt::com_ptr<IWICBitmapDecoder> decoder;
    if (FAILED(_factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.put()))) throw invalid_argument("The data is not an image.");

    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    winrt::check_hresult(decoder->GetFrame(0, frame.put()));

    winrt::com_ptr<IWICFormatConverter> converter;
    winrt::check_hresult(_factory->CreateFormatConverter(converter.put()));
    winrt::check_hresult(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));

    Pixels pixels;
    winrt::check_hresult(converter->GetSize(&pixels.Width, &pixels.Height));
    pixels.Bgra.resize(size_t(pixels.Width) * pixels.Height * 4);
    winrt::check_hresult(converter->CopyPixels(nullptr, pixels.Width * 4, UINT(pixels.Bgra.size()), pixels.Bgra.data()));
    return pixels;
  }
}
