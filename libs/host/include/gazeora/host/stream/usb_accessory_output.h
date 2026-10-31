// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>  // includes windows.h

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/frame_packet.h"

struct libusb_context;
struct libusb_device;

namespace gazeora {

class UsbAccessoryPipe;

// The USB protocol: frames to an Android phone on a USB cable, as GZF1 messages
// (frame_packet.h) over Android Open Accessory. Every second the output looks for a phone:
// one in accessory mode is opened, any other Android device is asked to switch to it (which
// makes it reconnect as the accessory and start the Gazeora app). The app says hello once it
// has opened the accessory; frames flow from then on.
//
// Windows needs drivers for this. To ask the phone to switch, libusb opens it in its normal
// mode: through WinUSB on one of its interfaces, which the adb interface has (USB debugging
// on, adb not holding it), or UsbDk. Then the accessory interface: through WinUSB, which
// Gazeora installs (apps/gazeora: platform/usb_driver.h) and opens directly, leaving adb its
// interface; or through libusb, with UsbDk or WinUSB from Zadig. Status() says what's
// missing, and NeedsUsbDriver() when it's Gazeora's driver.
class UsbAccessoryOutput : public FrameOutput {
public:
    // What the phone is told it's connected to; the Android app's accessory_filter.xml
    // matches the first two.
    static constexpr const char* kManufacturer = "Gazeora";
    static constexpr const char* kModel = "Gazeora Stream";

    explicit UsbAccessoryOutput(StreamLog& log);
    ~UsbAccessoryOutput() override;

    bool Start(in_addr address, uint16_t port, std::wstring& error) override;
    void Stop() override;
    uint16_t Port() const override { return 0; }
    int Viewers() const override { return m_watching ? 1 : 0; }
    bool TakeKeyFrameRequest() override { return m_keyFrameWanted.exchange(false); }
    void Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
              uint64_t now) override;
    std::wstring Url(const std::wstring&) const override { return {}; }
    std::wstring Status() const override;
    bool NeedsUsbDriver() const override;
    std::vector<UsbPhone> PhonesNeedingDriver() const override;
    void SetPointerSink(PointerEventSink* sink) override { m_pointerSink = sink; }
    void SetHeadPoseSink(HeadPoseSink* sink) override { m_headPoseSink = sink; }

private:
    // NoDriver: it opens, but its driver can't send the request (a phone without USB
    // debugging, with only Windows' file transfer driver).
    enum class Switch { Asked, NotAndroid, InUse, NoDriver, CannotOpen };

    void WatchLoop();
    void Scan();
    // Asks an Android device to reconnect as an accessory.
    Switch SwitchToAccessory(libusb_device* device);
    // Through libusb: 0, or the libusb error opening it.
    int OpenAccessory(libusb_device* device);
    void Connected(std::unique_ptr<UsbAccessoryPipe> pipe, std::string name);
    void ReadFromPhone(uint64_t now);
    void Hear(const FrameHeader& header, const uint8_t* payload, uint64_t now);
    // Pointer and head pose messages, on to their sinks.
    void HearInput(const FrameHeader& header, const uint8_t* payload);
    void Close(const char* reason);
    void WriteLoop();
    void SetStatus(std::wstring status);

    StreamLog& m_log;
    libusb_context* m_context = nullptr;
    std::thread m_watchThread;
    std::thread m_writeThread;
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_watching{false};  // the app said hello lately
    std::atomic<bool> m_keyFrameWanted{false};
    std::atomic<PointerEventSink*> m_pointerSink{nullptr};
    std::atomic<HeadPoseSink*> m_headPoseSink{nullptr};

    // The open accessory. The watch thread opens, reads and closes it; the writer writes.
    std::mutex m_deviceMutex;
    std::unique_ptr<UsbAccessoryPipe> m_pipe;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_needsDriver{false};

    // The watch thread's.
    std::set<std::string> m_asked;  // devices asked to switch, by port path, while plugged in
    std::string m_inbox;            // bytes from the phone not yet a whole message
    uint64_t m_lastHelloMs = 0;
    std::string m_phoneName;

    // Messages for the writer.
    std::mutex m_queueMutex;
    std::condition_variable m_queueReady;
    std::deque<std::string> m_queue;
    size_t m_queuedBytes = 0;

    // The encode thread's.
    bool m_waitingForKeyFrame = true;
    uint32_t m_frameNumber = 0;
    uint32_t m_framesSent = 0;
    uint32_t m_framesSkipped = 0;
    uint64_t m_countedSinceMs = 0;

    mutable std::mutex m_statusMutex;
    std::wstring m_status;
    std::vector<UsbPhone> m_phonesNeedingDriver;
};

}  // namespace gazeora
