// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <d3d11.h>
#include <mfobjects.h>
#include <wrl/client.h>

#include <string>

namespace gazeora {

// Adapter pattern: a D3D11 device as Media Foundation's IMFDXGIDeviceManager, which is how a
// GPU encoder gets the device whose textures it encodes. The device is made safe to use from
// the encoder's threads alongside the thread that draws the frames.
class DxgiDeviceManager {
public:
    bool Open(ID3D11Device* device, std::wstring& error);
    void Close();

    bool IsOpen() const { return m_manager != nullptr; }
    IMFDXGIDeviceManager* Get() const { return m_manager.Get(); }

private:
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> m_manager;
};

}  // namespace gazeora
