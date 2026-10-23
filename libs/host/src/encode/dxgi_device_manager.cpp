#include "gazeora/host/encode/dxgi_device_manager.h"

#include <d3d10.h>
#include <mfapi.h>

#include <cwchar>

using Microsoft::WRL::ComPtr;

namespace gazeora {

bool DxgiDeviceManager::Open(ID3D11Device* device, std::wstring& error) {
    Close();
    // The encoder runs the device from threads of its own while the capture thread draws.
    ComPtr<ID3D10Multithread> multithread;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&multithread)))) {
        error = L"the device can't be shared with the encoder";
        return false;
    }
    multithread->SetMultithreadProtected(TRUE);

    UINT token = 0;
    ComPtr<IMFDXGIDeviceManager> manager;
    HRESULT hr = MFCreateDXGIDeviceManager(&token, &manager);
    if (SUCCEEDED(hr)) {
        hr = manager->ResetDevice(device, token);
    }
    if (FAILED(hr)) {
        wchar_t text[80];
        swprintf(text, 80, L"sharing the device with the encoder failed (0x%08lX)",
                 static_cast<unsigned long>(hr));
        error = text;
        return false;
    }
    m_manager = manager;
    return true;
}

void DxgiDeviceManager::Close() { m_manager.Reset(); }

}  // namespace gazeora
