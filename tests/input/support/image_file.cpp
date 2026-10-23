#include "support/image_file.h"

#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <vector>

using Microsoft::WRL::ComPtr;

namespace gazeora::input::testing {

namespace {

void Check(HRESULT hr, const std::wstring& path) {
    if (FAILED(hr)) {
        throw InputError(L"Couldn't decode " + path);
    }
}

}  // namespace

CameraFrame ImageFile::Load(const std::wstring& path) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)),
          path);
    ComPtr<IWICBitmapDecoder> decoder;
    Check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                             WICDecodeMetadataCacheOnDemand, &decoder),
          path);
    ComPtr<IWICBitmapFrameDecode> frame;
    Check(decoder->GetFrame(0, &frame), path);
    ComPtr<IWICFormatConverter> converter;
    Check(factory->CreateFormatConverter(&converter), path);
    Check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone,
                                nullptr, 0.0, WICBitmapPaletteTypeCustom),
          path);
    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);
    UINT stride = width * CameraFrame::kBytesPerPixel;
    std::vector<uint8_t> pixels(size_t(stride) * height);
    Check(converter->CopyPixels(nullptr, stride, UINT(pixels.size()), pixels.data()), path);
    return CameraFrame(int(width), int(height), std::move(pixels));
}

std::wstring ImageFile::TestData(const wchar_t* name) {
    return std::wstring(GAZEORA_INPUT_TEST_DATA) + L"/" + name;
}

}  // namespace gazeora::input::testing
