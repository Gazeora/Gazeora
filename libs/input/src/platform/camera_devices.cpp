#include "platform/camera_devices.h"

#include <mfapi.h>

#include "gazeora/input/exceptions/input_error.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::input {

namespace {

// The IMFActivate array MFEnumDeviceSources hands out, released together.
class DeviceArray {
public:
    DeviceArray() {
        ComPtr<IMFAttributes> filter;
        if (SUCCEEDED(MFCreateAttributes(&filter, 1)) &&
            SUCCEEDED(filter->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
            MFEnumDeviceSources(filter.Get(), &m_devices, &m_count);
        }
    }
    ~DeviceArray() {
        for (UINT32 i = 0; i < m_count; ++i) {
            m_devices[i]->Release();
        }
        CoTaskMemFree(m_devices);
    }
    DeviceArray(const DeviceArray&) = delete;
    DeviceArray& operator=(const DeviceArray&) = delete;

    UINT32 Count() const { return m_count; }
    IMFActivate* At(UINT32 index) const { return m_devices[index]; }

private:
    IMFActivate** m_devices = nullptr;
    UINT32 m_count = 0;
};

std::wstring StringOf(IMFActivate* device, const GUID& key) {
    wchar_t* text = nullptr;
    UINT32 length = 0;
    if (FAILED(device->GetAllocatedString(key, &text, &length))) {
        return {};
    }
    std::wstring copy(text, length);
    CoTaskMemFree(text);
    return copy;
}

CameraInfo InfoOf(IMFActivate* device) {
    return {StringOf(device, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK),
            StringOf(device, MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME)};
}

IMFActivate* Find(const DeviceArray& devices, const std::wstring& id) {
    for (UINT32 i = 0; i < devices.Count(); ++i) {
        if (id.empty() || InfoOf(devices.At(i)).id == id) {
            return devices.At(i);
        }
    }
    throw InputError(devices.Count() == 0 ? L"No camera is connected."
                                          : L"The chosen camera isn't connected.");
}

}  // namespace

std::vector<CameraInfo> CameraDevices::List() {
    DeviceArray devices;
    std::vector<CameraInfo> cameras;
    for (UINT32 i = 0; i < devices.Count(); ++i) {
        cameras.push_back(InfoOf(devices.At(i)));
    }
    return cameras;
}

ComPtr<IMFMediaSource> CameraDevices::Activate(const std::wstring& id) {
    DeviceArray devices;
    ComPtr<IMFMediaSource> source;
    HRESULT hr = Find(devices, id)->ActivateObject(IID_PPV_ARGS(&source));
    if (hr == E_ACCESSDENIED) {
        throw InputError(
            L"Windows blocks the camera. Turn on Settings > Privacy & security > "
            L"Camera > Let desktop apps access your camera.");
    }
    if (FAILED(hr)) {
        throw InputError(L"The camera couldn't be opened. Is another app using it?");
    }
    return source;
}

}  // namespace gazeora::input
