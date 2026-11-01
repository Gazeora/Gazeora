// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>  // includes windows.h, so first

#include <libusb.h>
#include <setupapi.h>
#include <winusb.h>

#include <memory>

namespace gazeora {

// RAII: the libusb, SetupAPI and WinUSB handles the USB plugin opens, each released however
// the scope that holds it ends, an exception included.

struct LibusbDeviceListFree {
    void operator()(libusb_device** list) const { libusb_free_device_list(list, 1); }
};
using UniqueDeviceList = std::unique_ptr<libusb_device*, LibusbDeviceListFree>;

struct LibusbConfigFree {
    void operator()(libusb_config_descriptor* config) const {
        libusb_free_config_descriptor(config);
    }
};
using UniqueConfig = std::unique_ptr<libusb_config_descriptor, LibusbConfigFree>;

struct LibusbClose {
    void operator()(libusb_device_handle* handle) const { libusb_close(handle); }
};
using UniqueDeviceHandle = std::unique_ptr<libusb_device_handle, LibusbClose>;

// A handle with interface 0 claimed: let go of it, then closed.
struct LibusbReleaseAndClose {
    void operator()(libusb_device_handle* handle) const {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
    }
};
using UniqueClaimedHandle = std::unique_ptr<libusb_device_handle, LibusbReleaseAndClose>;

struct DeviceInfoListDestroy {
    void operator()(HDEVINFO set) const { SetupDiDestroyDeviceInfoList(set); }
};
using UniqueDeviceInfoList = std::unique_ptr<void, DeviceInfoListDestroy>;

struct HandleClose {
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};
using UniqueFile = std::unique_ptr<void, HandleClose>;

struct WinUsbFree {
    void operator()(WINUSB_INTERFACE_HANDLE usb) const { WinUsb_Free(usb); }
};
using UniqueWinUsb = std::unique_ptr<void, WinUsbFree>;

// The device's active configuration, or null if it can't be read.
inline UniqueConfig ActiveConfig(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    return UniqueConfig(libusb_get_active_config_descriptor(device, &config) == 0 ? config
                                                                                  : nullptr);
}

// A SetupAPI device set, or null if it can't be had.
inline UniqueDeviceInfoList DeviceInfoList(HDEVINFO set) {
    return UniqueDeviceInfoList(set == INVALID_HANDLE_VALUE ? nullptr : set);
}

}  // namespace gazeora
