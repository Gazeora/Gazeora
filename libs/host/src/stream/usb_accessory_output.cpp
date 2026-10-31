// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/usb_accessory_output.h"

#include <libusb.h>
#include <setupapi.h>
#include <winusb.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "gazeora/host/stream/head_pose_message.h"
#include "gazeora/host/stream/pointer_message.h"

namespace gazeora {

// The open accessory interface: its bulk endpoint each way. Results are libusb's codes.
class UsbAccessoryPipe {
public:
    virtual ~UsbAccessoryPipe() = default;
    virtual int Read(uint8_t* buffer, int size, int& got) = 0;
    virtual int Write(const uint8_t* data, int size, int& sent) = 0;
};

namespace {

// Android Open Accessory: the control requests, and the IDs a phone reconnects with.
constexpr uint8_t kAoaGetProtocol = 51;
constexpr uint8_t kAoaSendString = 52;
constexpr uint8_t kAoaStart = 53;
constexpr uint16_t kAccessoryVendor = 0x18D1;
constexpr uint16_t kAccessoryProductFirst = 0x2D00;  // 2D00-2D05: accessory, with adb, audio
constexpr uint16_t kAccessoryProductLast = 0x2D05;
constexpr uint16_t kAppleVendor = 0x05AC;
constexpr unsigned kControlTimeoutMs = 1000;
// A write the app doesn't take in this long is dropped: it isn't reading.
constexpr unsigned kWriteTimeoutMs = 1000;
constexpr unsigned kReadTimeoutMs = 200;
constexpr uint64_t kScanEveryMs = 1000;
// A frame is skipped while this much waits for the cable: about half a second at the full
// bitrate.
constexpr size_t kBacklogBytes = 512 * 1024;

std::wstring Widen(const std::string& text) { return std::wstring(text.begin(), text.end()); }

// "bus-port.port...", which stays the same while a device is plugged into the same socket,
// across its switch to accessory mode.
std::string PortPath(libusb_device* device) {
    uint8_t ports[8];
    int count = libusb_get_port_numbers(device, ports, sizeof(ports));
    std::string path = std::to_string(libusb_get_bus_number(device));
    for (int i = 0; i < count; ++i) {
        path += (i == 0 ? "-" : ".") + std::to_string(ports[i]);
    }
    return path;
}

enum class Looks { NotAndroid, MaybeAndroid, LikeAPhone };

// Phones show an MTP/PTP interface (class 6), or vendor-specific ones (adb, MTP on some, the
// charge-only interface). Hubs, keyboards, mice and the like don't get asked.
Looks LooksLike(libusb_device* device, const libusb_device_descriptor& descriptor) {
    if (descriptor.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE &&
        descriptor.bDeviceClass != LIBUSB_CLASS_MISCELLANEOUS &&
        descriptor.bDeviceClass != LIBUSB_CLASS_VENDOR_SPEC) {
        return Looks::NotAndroid;
    }
    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(device, &config) != 0) {
        return Looks::NotAndroid;
    }
    Looks looks = Looks::NotAndroid;
    for (int i = 0; i < config->bNumInterfaces; ++i) {
        for (int a = 0; a < config->interface[i].num_altsetting; ++a) {
            uint8_t cls = config->interface[i].altsetting[a].bInterfaceClass;
            if (cls == LIBUSB_CLASS_IMAGE) {
                looks = Looks::LikeAPhone;
            } else if (cls == LIBUSB_CLASS_VENDOR_SPEC && looks == Looks::NotAndroid) {
                looks = Looks::MaybeAndroid;
            }
        }
    }
    libusb_free_config_descriptor(config);
    return looks;
}

std::string DeviceName(libusb_device_handle* handle, const libusb_device_descriptor& descriptor) {
    auto text = [&](uint8_t index) {
        unsigned char buffer[128] = {};
        if (index == 0 ||
            libusb_get_string_descriptor_ascii(handle, index, buffer, sizeof(buffer)) <= 0) {
            return std::string();
        }
        return std::string(reinterpret_cast<char*>(buffer));
    };
    std::string name = text(descriptor.iManufacturer);
    std::string product = text(descriptor.iProduct);
    if (!product.empty()) {
        name += (name.empty() ? "" : " ") + product;
    }
    if (name.empty()) {
        char ids[16];
        snprintf(ids, sizeof(ids), "%04X:%04X", descriptor.idVendor, descriptor.idProduct);
        name = ids;
    }
    return name;
}

// The hardware ID Windows matches a driver to: the device's, or for a device with several
// interfaces the file transfer one's (MTP/PTP).
std::wstring PhoneHardwareId(libusb_device* device, const libusb_device_descriptor& descriptor) {
    wchar_t id[64];
    swprintf(id, 64, L"USB\\VID_%04X&PID_%04X", descriptor.idVendor, descriptor.idProduct);
    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(device, &config) != 0) {
        return id;
    }
    std::wstring hardwareId = id;
    if (config->bNumInterfaces > 1) {
        for (int i = 0; i < config->bNumInterfaces; ++i) {
            const libusb_interface_descriptor& interface = config->interface[i].altsetting[0];
            if (interface.bInterfaceClass == LIBUSB_CLASS_IMAGE) {
                swprintf(id, 64, L"&MI_%02X", interface.bInterfaceNumber);
                hardwareId += id;
                break;
            }
        }
    }
    libusb_free_config_descriptor(config);
    return hardwareId;
}

// What Windows calls the device (its friendly name, e.g. "Redmi Note 11T 5G").
std::wstring WindowsDeviceName(const libusb_device_descriptor& descriptor) {
    wchar_t prefix[32];
    swprintf(prefix, 32, L"USB\\VID_%04X&PID_%04X", descriptor.idVendor, descriptor.idProduct);
    std::wstring name;
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        return name;
    }
    SP_DEVINFO_DATA device = {sizeof(device)};
    for (DWORD i = 0; name.empty() && SetupDiEnumDeviceInfo(set, i, &device); ++i) {
        wchar_t ids[512] = {};
        if (!SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_HARDWAREID, nullptr,
                                               reinterpret_cast<BYTE*>(ids), sizeof(ids) - 4,
                                               nullptr) ||
            _wcsicmp(ids, prefix) != 0 &&
                _wcsnicmp(ids, (std::wstring(prefix) + L"&REV").c_str(), wcslen(prefix) + 4) != 0) {
            continue;
        }
        wchar_t text[256] = {};
        if (SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_FRIENDLYNAME, nullptr,
                                              reinterpret_cast<BYTE*>(text), sizeof(text) - 2,
                                              nullptr) ||
            SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_DEVICEDESC, nullptr,
                                              reinterpret_cast<BYTE*>(text), sizeof(text) - 2,
                                              nullptr)) {
            name = text;
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return name;
}

bool IsAccessory(const libusb_device_descriptor& descriptor) {
    return descriptor.idVendor == kAccessoryVendor &&
           descriptor.idProduct >= kAccessoryProductFirst &&
           descriptor.idProduct <= kAccessoryProductLast;
}

// Through libusb: with UsbDk, or WinUSB from Zadig. libusb opens every WinUSB interface of the
// phone, adb's too, so adb loses the phone meanwhile.
class LibusbPipe : public UsbAccessoryPipe {
public:
    LibusbPipe(libusb_device_handle* handle, uint8_t in, uint8_t out)
        : m_handle(handle), m_in(in), m_out(out) {}
    ~LibusbPipe() override {
        libusb_release_interface(m_handle, 0);
        libusb_close(m_handle);
    }
    int Read(uint8_t* buffer, int size, int& got) override {
        return libusb_bulk_transfer(m_handle, m_in, buffer, size, &got, kReadTimeoutMs);
    }
    int Write(const uint8_t* data, int size, int& sent) override {
        return libusb_bulk_transfer(m_handle, m_out, const_cast<uint8_t*>(data), size, &sent,
                                    kWriteTimeoutMs);
    }

private:
    libusb_device_handle* m_handle;
    uint8_t m_in;
    uint8_t m_out;
};

// Straight through WinUSB, on the interface the driver Gazeora installs registers
// (apps/gazeora: platform/usb_driver.cpp, whose INF names this GUID): the accessory interface
// alone, so adb keeps the phone's other one.
const GUID kAccessoryInterface = {
    0x6A8E2B7C, 0x3F41, 0x4C9B, {0x9D, 0x0E, 0x7B, 0x2F, 0x5A, 0x1C, 0x8E, 0x43}};

class WinUsbPipe : public UsbAccessoryPipe {
public:
    // Opens the first phone in accessory mode with Gazeora's driver. 0, LIBUSB_ERROR_NOT_FOUND
    // if there's none, or the error opening it.
    static int Open(std::unique_ptr<UsbAccessoryPipe>& pipe, std::string& name) {
        HDEVINFO set = SetupDiGetClassDevsW(&kAccessoryInterface, nullptr, nullptr,
                                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (set == INVALID_HANDLE_VALUE) {
            return LIBUSB_ERROR_NOT_FOUND;
        }
        int result = LIBUSB_ERROR_NOT_FOUND;
        SP_DEVICE_INTERFACE_DATA data = {sizeof(data)};
        for (DWORD i = 0; result != 0 &&
                          SetupDiEnumDeviceInterfaces(set, nullptr, &kAccessoryInterface, i, &data);
             ++i) {
            DWORD size = 0;
            SetupDiGetDeviceInterfaceDetailW(set, &data, nullptr, 0, &size, nullptr);
            std::vector<BYTE> buffer(size);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (size == 0 ||
                !SetupDiGetDeviceInterfaceDetailW(set, &data, detail, size, nullptr, nullptr)) {
                continue;
            }
            result = OpenPath(detail->DevicePath, pipe, name);
        }
        SetupDiDestroyDeviceInfoList(set);
        return result;
    }

    ~WinUsbPipe() override {
        WinUsb_Free(m_usb);
        CloseHandle(m_file);
    }

    int Read(uint8_t* buffer, int size, int& got) override {
        ULONG done = 0;
        bool ok = WinUsb_ReadPipe(m_usb, m_in, buffer, ULONG(size), &done, nullptr);
        got = int(done);
        return ok ? 0 : Error(GetLastError());
    }

    int Write(const uint8_t* data, int size, int& sent) override {
        ULONG done = 0;
        bool ok =
            WinUsb_WritePipe(m_usb, m_out, const_cast<PUCHAR>(data), ULONG(size), &done, nullptr);
        sent = int(done);
        return ok ? 0 : Error(GetLastError());
    }

private:
    WinUsbPipe(HANDLE file, WINUSB_INTERFACE_HANDLE usb, UCHAR in, UCHAR out)
        : m_file(file), m_usb(usb), m_in(in), m_out(out) {}

    static int OpenPath(const wchar_t* path, std::unique_ptr<UsbAccessoryPipe>& pipe,
                        std::string& name) {
        HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return GetLastError() == ERROR_ACCESS_DENIED ? LIBUSB_ERROR_ACCESS : LIBUSB_ERROR_IO;
        }
        WINUSB_INTERFACE_HANDLE usb = nullptr;
        if (!WinUsb_Initialize(file, &usb)) {
            CloseHandle(file);
            return LIBUSB_ERROR_IO;
        }
        // One bulk endpoint each way.
        UCHAR in = 0, out = 0;
        USB_INTERFACE_DESCRIPTOR interface = {};
        if (WinUsb_QueryInterfaceSettings(usb, 0, &interface)) {
            for (UCHAR p = 0; p < interface.bNumEndpoints; ++p) {
                WINUSB_PIPE_INFORMATION info = {};
                if (WinUsb_QueryPipe(usb, 0, p, &info) && info.PipeType == UsbdPipeTypeBulk) {
                    (USB_ENDPOINT_DIRECTION_IN(info.PipeId) ? in : out) = info.PipeId;
                }
            }
        }
        if (!in || !out) {
            WinUsb_Free(usb);
            CloseHandle(file);
            return LIBUSB_ERROR_NOT_FOUND;
        }
        ULONG readTimeout = kReadTimeoutMs, writeTimeout = kWriteTimeoutMs;
        UCHAR yes = TRUE;
        WinUsb_SetPipePolicy(usb, in, PIPE_TRANSFER_TIMEOUT, sizeof(readTimeout), &readTimeout);
        WinUsb_SetPipePolicy(usb, out, PIPE_TRANSFER_TIMEOUT, sizeof(writeTimeout), &writeTimeout);
        // A message ending on a packet boundary still ends the phone's read.
        WinUsb_SetPipePolicy(usb, out, SHORT_PACKET_TERMINATE, sizeof(yes), &yes);
        name = Name(usb);
        pipe.reset(new WinUsbPipe(file, usb, in, out));
        return 0;
    }

    // "Manufacturer Product", from the device's string descriptors.
    static std::string Name(WINUSB_INTERFACE_HANDLE usb) {
        UCHAR device[18] = {};
        ULONG got = 0;
        if (!WinUsb_GetDescriptor(usb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, device, sizeof(device),
                                  &got) ||
            got < sizeof(device)) {
            return "the phone";
        }
        auto text = [&](UCHAR index) {
            UCHAR buffer[256] = {};
            ULONG length = 0;
            if (index == 0 ||
                !WinUsb_GetDescriptor(usb, USB_STRING_DESCRIPTOR_TYPE, index, 0x0409, buffer,
                                      sizeof(buffer), &length) ||
                length < 2) {
                return std::string();
            }
            std::string ascii;  // UTF-16LE after the length and type
            for (ULONG i = 2; i + 1 < length && i < buffer[0]; i += 2) {
                ascii += buffer[i + 1] == 0 && buffer[i] < 0x80 ? char(buffer[i]) : '?';
            }
            return ascii;
        };
        std::string name = text(device[14]);
        std::string product = text(device[15]);
        if (!product.empty()) {
            name += (name.empty() ? "" : " ") + product;
        }
        return name.empty() ? "the phone" : name;
    }

    static int Error(DWORD code) {
        switch (code) {
            case ERROR_SEM_TIMEOUT:
                return LIBUSB_ERROR_TIMEOUT;
            case ERROR_DEVICE_NOT_CONNECTED:
            case ERROR_BAD_COMMAND:
            case ERROR_GEN_FAILURE:
            case ERROR_FILE_NOT_FOUND:
                return LIBUSB_ERROR_NO_DEVICE;
            default:
                return LIBUSB_ERROR_IO;
        }
    }

    HANDLE m_file;
    WINUSB_INTERFACE_HANDLE m_usb;
    UCHAR m_in;
    UCHAR m_out;
};

}  // namespace

UsbAccessoryOutput::UsbAccessoryOutput(StreamLog& log) : m_log(log) {}

UsbAccessoryOutput::~UsbAccessoryOutput() { Stop(); }

bool UsbAccessoryOutput::Start(in_addr, uint16_t, std::wstring& error) {
    // UsbDk reaches any device without replacing its driver; without it, libusb uses WinUSB.
    libusb_init_option useUsbDk = {};
    useUsbDk.option = LIBUSB_OPTION_USE_USBDK;
    const char* backend = "UsbDk";
    int result = libusb_init_context(&m_context, &useUsbDk, 1);
    if (result != 0) {
        backend = "WinUSB";
        result = libusb_init_context(&m_context, nullptr, 0);
    }
    if (result != 0) {
        error = L"USB is unavailable: " + Widen(libusb_strerror(result));
        m_context = nullptr;
        return false;
    }
    m_log.Write("USB accessory output, through %s", backend);
    m_stopping = false;
    SetStatus(L"Looking for a phone on USB");
    m_watchThread = std::thread([this] { WatchLoop(); });
    m_writeThread = std::thread([this] { WriteLoop(); });
    return true;
}

void UsbAccessoryOutput::Stop() {
    m_stopping = true;
    m_queueReady.notify_all();
    if (m_watchThread.joinable()) {
        m_watchThread.join();
    }
    if (m_writeThread.joinable()) {
        m_writeThread.join();
    }
    Close("the stream stopped");
    if (m_context) {
        libusb_exit(m_context);
        m_context = nullptr;
    }
    m_asked.clear();
}

std::wstring UsbAccessoryOutput::Status() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

bool UsbAccessoryOutput::NeedsUsbDriver() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_needsDriver || !m_phonesNeedingDriver.empty();
}

std::vector<UsbPhone> UsbAccessoryOutput::PhonesNeedingDriver() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_phonesNeedingDriver;
}

void UsbAccessoryOutput::SetStatus(std::wstring status) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status = std::move(status);
}

void UsbAccessoryOutput::WatchLoop() {
    uint64_t nextScanMs = 0;
    while (!m_stopping) {
        uint64_t now = GetTickCount64();
        if (m_connected) {
            ReadFromPhone(now);
            continue;
        }
        if (now >= nextScanMs) {
            Scan();
            nextScanMs = now + kScanEveryMs;
        }
        if (!m_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void UsbAccessoryOutput::Scan() {
    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(m_context, &list);
    if (count < 0) {
        return;
    }
    std::set<std::string> present;
    bool needsDriver = false;      // a phone in accessory mode, its interface without WinUSB
    bool inUse = false;            // an Android device another program has open: adb, usually
    bool unreachable = false;      // a likely phone libusb can't open in its normal mode
    std::vector<UsbPhone> phones;  // phones whose driver can't send the switch request
    std::vector<libusb_device*> accessories;
    for (ssize_t i = 0; i < count && !m_connected; ++i) {
        libusb_device* device = list[i];
        libusb_device_descriptor descriptor;
        if (libusb_get_device_descriptor(device, &descriptor) != 0) {
            continue;
        }
        std::string path = PortPath(device);
        present.insert(path);
        if (IsAccessory(descriptor)) {
            accessories.push_back(device);  // opened below
        } else if (!m_asked.count(path)) {
            Looks looks = LooksLike(device, descriptor);
            if (looks == Looks::NotAndroid) {
                continue;
            }
            switch (SwitchToAccessory(device)) {
                case Switch::Asked:
                case Switch::NotAndroid:
                    m_asked.insert(path);
                    break;
                case Switch::InUse:
                    inUse = true;  // tried again next time
                    break;
                case Switch::NoDriver:
                    // Tried again next time: the driver may be installed meanwhile. Not
                    // offered for Apple devices, or anything but a phone (a file transfer
                    // interface), as WinUSB on them would only cost them their own driver.
                    if (looks == Looks::LikeAPhone && descriptor.idVendor != kAppleVendor) {
                        phones.push_back(
                            {PhoneHardwareId(device, descriptor), WindowsDeviceName(descriptor)});
                        if (phones.back().name.empty()) {
                            phones.back().name = L"the phone";
                        }
                    }
                    break;
                case Switch::CannotOpen:
                    // Tried again next time: a driver may turn up.
                    unreachable |= looks == Looks::LikeAPhone;
                    break;
            }
        }
    }
    if (!accessories.empty() && !m_connected) {
        // Through Gazeora's driver, which leaves adb its interface; without it, through
        // libusb (UsbDk, or WinUSB from Zadig).
        std::unique_ptr<UsbAccessoryPipe> pipe;
        std::string name;
        int result = WinUsbPipe::Open(pipe, name);
        if (result == 0) {
            Connected(std::move(pipe), name);
        } else if (result == LIBUSB_ERROR_NOT_FOUND) {
            for (libusb_device* device : accessories) {
                result = OpenAccessory(device);
                if (result == 0) {
                    break;
                }
                needsDriver |=
                    result == LIBUSB_ERROR_NOT_SUPPORTED || result == LIBUSB_ERROR_NOT_FOUND;
                inUse |= result == LIBUSB_ERROR_ACCESS || result == LIBUSB_ERROR_BUSY;
            }
        } else {
            inUse |= result == LIBUSB_ERROR_ACCESS;
        }
    }
    libusb_free_device_list(list, 1);
    // Unplugged devices get asked again when they're back.
    for (auto it = m_asked.begin(); it != m_asked.end();) {
        it = present.count(*it) ? std::next(it) : m_asked.erase(it);
    }
    m_needsDriver = needsDriver;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_phonesNeedingDriver = phones;
    }
    if (!m_connected) {
        if (needsDriver) {
            SetStatus(
                L"The phone is in accessory mode, but Windows has no driver for its "
                L"accessory interface yet. Install the USB driver below, once.");
        } else if (!phones.empty()) {
            SetStatus(L"Gazeora can't switch " + phones.front().name +
                      L" to accessory mode with the driver Windows gave it. Install the USB "
                      L"driver below for it, or turn on USB debugging on the phone.");
        } else if (inUse) {
            SetStatus(
                L"Another program has the phone open, adb (Android Studio) most likely. "
                L"Gazeora needs it for a moment to switch the phone to accessory mode: "
                L"run \"adb kill-server\", or unplug the phone and plug it in again with "
                L"adb closed. Once switched, adb works alongside.");
        } else if (unreachable) {
            SetStatus(
                L"A phone is connected, but Gazeora can't reach it in its normal USB "
                L"mode. Unplug it and plug it in again; if that doesn't help, turn on USB "
                L"debugging on the phone.");
        } else {
            SetStatus(L"Looking for a phone on USB");
        }
    }
}

UsbAccessoryOutput::Switch UsbAccessoryOutput::SwitchToAccessory(libusb_device* device) {
    libusb_device_handle* handle = nullptr;
    int opened = libusb_open(device, &handle);
    if (opened != 0) {
        // WinUSB lets one program at a time open a device.
        if (opened == LIBUSB_ERROR_ACCESS || opened == LIBUSB_ERROR_BUSY) {
            return Switch::InUse;
        }
        return opened == LIBUSB_ERROR_NOT_SUPPORTED || opened == LIBUSB_ERROR_NOT_FOUND
                   ? Switch::NoDriver
                   : Switch::CannotOpen;
    }
    libusb_device_descriptor descriptor;
    libusb_get_device_descriptor(device, &descriptor);
    uint8_t version[2] = {};
    int got = libusb_control_transfer(
        handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        kAoaGetProtocol, 0, 0, version, sizeof(version), kControlTimeoutMs);
    if (got == LIBUSB_ERROR_NOT_SUPPORTED || got == LIBUSB_ERROR_NOT_FOUND) {
        libusb_close(handle);
        return Switch::NoDriver;  // its driver (Windows' file transfer one) can't send it
    }
    bool android = got == 2 && (version[0] | version[1] << 8) >= 1;
    if (android) {
        std::string name = DeviceName(handle, descriptor);
        // Manufacturer, model, description, version, URI, serial.
        const char* strings[] = {kManufacturer, kModel, "Gazeora game stream", "1", "", "1"};
        bool sent = true;
        for (uint16_t index = 0; index < 6 && sent; ++index) {
            sent = libusb_control_transfer(
                       handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, kAoaSendString, 0,
                       index, reinterpret_cast<unsigned char*>(const_cast<char*>(strings[index])),
                       uint16_t(strlen(strings[index]) + 1), kControlTimeoutMs) >= 0;
        }
        sent = sent &&
               libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                       kAoaStart, 0, 0, nullptr, 0, kControlTimeoutMs) >= 0;
        m_log.Write(sent ? "asked %s to switch to USB accessory mode"
                         : "%s didn't take the switch to USB accessory mode",
                    name.c_str());
        if (sent) {
            SetStatus(L"Switching " + Widen(name) + L" to accessory mode");
        }
    }
    libusb_close(handle);
    return android ? Switch::Asked : Switch::NotAndroid;
}

int UsbAccessoryOutput::OpenAccessory(libusb_device* device) {
    libusb_device_handle* handle = nullptr;
    int result = libusb_open(device, &handle);
    if (result != 0) {
        return result;
    }
    result = libusb_claim_interface(handle, 0);
    if (result != 0) {
        libusb_close(handle);
        return result;
    }
    // The accessory interface has one bulk endpoint each way.
    uint8_t in = 0, out = 0;
    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(device, &config) == 0) {
        const libusb_interface_descriptor& interface = config->interface[0].altsetting[0];
        for (int e = 0; e < interface.bNumEndpoints; ++e) {
            const libusb_endpoint_descriptor& endpoint = interface.endpoint[e];
            if ((endpoint.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                ((endpoint.bEndpointAddress & LIBUSB_ENDPOINT_IN) ? in : out) =
                    endpoint.bEndpointAddress;
            }
        }
        libusb_free_config_descriptor(config);
    }
    if (!in || !out) {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
        return LIBUSB_ERROR_NOT_FOUND;
    }
    libusb_device_descriptor descriptor;
    libusb_get_device_descriptor(device, &descriptor);
    Connected(std::make_unique<LibusbPipe>(handle, in, out), DeviceName(handle, descriptor));
    return 0;
}

void UsbAccessoryOutput::Connected(std::unique_ptr<UsbAccessoryPipe> pipe, std::string name) {
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        m_pipe = std::move(pipe);
    }
    m_phoneName = std::move(name);
    m_inbox.clear();
    m_lastHelloMs = 0;
    m_connected = true;
    m_log.Write("%s connected as a USB accessory", m_phoneName.c_str());
    SetStatus(L"Connected to " + Widen(m_phoneName) + L". Waiting for the Gazeora app on it");
}

void UsbAccessoryOutput::Hear(const FrameHeader& header, const uint8_t* payload, uint64_t now) {
    switch (header.type) {
        case FrameMessage::Hello:
            if (!m_watching) {
                m_log.Write("the Gazeora app on %s said hello: streaming to it",
                            m_phoneName.c_str());
                m_keyFrameWanted = true;
            }
            m_watching = true;
            m_lastHelloMs = now;
            SetStatus(L"Streaming to " + Widen(m_phoneName));
            break;
        case FrameMessage::KeyFrameRequest:
            m_keyFrameWanted = true;
            break;
        case FrameMessage::Bye:
            m_watching = false;
            break;
        default:
            HearInput(header, payload);
            break;
    }
}

void UsbAccessoryOutput::HearInput(const FrameHeader& header, const uint8_t* payload) {
    PointerEventSink* pointer = m_pointerSink;
    HeadPoseSink* head = m_headPoseSink;
    PointerEvent event;
    HeadPose pose;
    if (header.type == FrameMessage::Pointer && pointer &&
        PointerMessage::Read(payload, header.tail, event)) {
        pointer->OnPointer(event);
    } else if (header.type == FrameMessage::HeadPose && head &&
               HeadPoseMessage::Read(payload, header.tail, pose)) {
        head->OnHeadPose(pose);
    }
}

void UsbAccessoryOutput::ReadFromPhone(uint64_t now) {
    uint8_t buffer[16384];  // accessory reads come in whole USB transfers
    int got = 0;
    int result = m_pipe->Read(buffer, sizeof(buffer), got);
    if (result != 0 && result != LIBUSB_ERROR_TIMEOUT) {
        Close(result == LIBUSB_ERROR_NO_DEVICE ? "the phone was unplugged" : "reading failed");
        SetStatus(L"Looking for a phone on USB");
        return;
    }
    m_inbox.append(reinterpret_cast<char*>(buffer), size_t(got));
    FrameHeader header;
    while (header.Read(reinterpret_cast<const uint8_t*>(m_inbox.data()), m_inbox.size()) &&
           m_inbox.size() >= kFrameHeaderSize + header.tail) {
        Hear(header, reinterpret_cast<const uint8_t*>(m_inbox.data()) + kFrameHeaderSize, now);
        m_inbox.erase(0, kFrameHeaderSize + header.tail);
    }
    if (m_inbox.size() >= kFrameHeaderSize &&
        !header.Read(reinterpret_cast<const uint8_t*>(m_inbox.data()), m_inbox.size())) {
        m_inbox.clear();  // lost track of where messages start
    }
    if (m_watching && now - m_lastHelloMs > kPlayerTimeoutMs) {
        m_log.Write("the Gazeora app on %s stopped saying hello", m_phoneName.c_str());
        m_watching = false;
    }
    if (!m_watching && m_connected) {
        SetStatus(L"Connected to " + Widen(m_phoneName) + L". Waiting for the Gazeora app on it");
    }
}

void UsbAccessoryOutput::Close(const char* reason) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    if (m_pipe) {
        m_log.Write("%s disconnected: %s", m_phoneName.c_str(), reason);
        m_pipe.reset();
    }
    m_connected = false;
    m_watching = false;
    std::lock_guard<std::mutex> queueLock(m_queueMutex);
    m_queue.clear();
    m_queuedBytes = 0;
}

void UsbAccessoryOutput::Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
                              uint64_t now) {
    if (!m_watching) {
        m_waitingForKeyFrame = true;
        return;
    }
    bool full;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        full = m_queuedBytes > kBacklogBytes;
    }
    if (full || (m_waitingForKeyFrame && !frame.keyFrame)) {
        if (full) {
            m_waitingForKeyFrame = true;
            ++m_framesSkipped;
        } else {
            m_keyFrameWanted = true;
        }
    } else {
        m_waitingForKeyFrame = false;
        ++m_framesSent;
        FrameHeader header;
        header.flags = uint8_t((frame.keyFrame ? kFrameFlagKey : 0) |
                               (frame.codec == VideoCodec::H265 ? kFrameFlagH265 : 0));
        header.width = uint16_t(frame.width);
        header.height = uint16_t(frame.height);
        header.frameNumber = m_frameNumber++;
        header.timestamp = frame.timestamp;
        std::string message = StreamMessage(header, frame.annexB);
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queuedBytes += message.size();
        m_queue.push_back(std::move(message));
        m_queueReady.notify_one();
    }
    if (now - m_countedSinceMs >= 1000) {
        uint32_t frames = m_framesSent + m_framesSkipped;
        bitrate.Report(0, frames ? float(m_framesSkipped) / float(frames) : 0.0f, now);
        if (m_framesSkipped > 0) {
            m_log.Write("USB skipped %u of %u frames: the phone can't keep up", m_framesSkipped,
                        frames);
        }
        m_framesSent = m_framesSkipped = 0;
        m_countedSinceMs = now;
    }
}

void UsbAccessoryOutput::WriteLoop() {
    while (!m_stopping) {
        std::string message;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueReady.wait_for(lock, std::chrono::milliseconds(250),
                                  [this] { return m_stopping || !m_queue.empty(); });
            if (m_queue.empty()) {
                continue;
            }
            message = std::move(m_queue.front());
            m_queue.pop_front();
            m_queuedBytes -= message.size();
        }
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        if (!m_pipe) {
            continue;
        }
        int sent = 0;
        int result = m_pipe->Write(reinterpret_cast<const uint8_t*>(message.data()),
                                   int(message.size()), sent);
        if (result == LIBUSB_ERROR_TIMEOUT) {
            // Half a message would leave the app out of step: it has to start over on a key
            // frame, and that's what the next hello's key frame request gets it.
            m_log.Write("USB write timed out after %d of %zu bytes", sent, message.size());
            m_watching = false;
        }
    }
}

}  // namespace gazeora
