#pragma once

#include <winsock2.h>  // includes windows.h

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/stream/bitrate_controller.h"
#include "gazeora/host/stream/pointer_event_sink.h"
#include "gazeora/host/stream/stream_log.h"
#include "gazeora/host/stream/stream_protocol.h"
#include "gazeora/host/stream/video_pipeline.h"

namespace gazeora {

// A phone on USB that Gazeora can't ask to switch to accessory mode: Windows' driver for it
// can't send the request. WinUSB on it can (apps/gazeora: platform/usb_driver.h).
struct UsbPhone {
    std::wstring hardwareId;  // e.g. USB\VID_2717&PID_FF40, or ...&MI_00 for one interface
    std::wstring name;        // as Windows calls it
};

// Where FrameStreamServer hands encoded frames for a protocol other than RTSP: UDP, HTTP or
// USB. Each output runs its own threads for its players; Send runs on the encode thread.
class FrameOutput {
public:
    virtual ~FrameOutput() = default;

    // Starts taking players on `address`, on `port` if it's free (0: any). Outputs without
    // a port ignore both. False with `error` set if it can't.
    virtual bool Start(in_addr address, uint16_t port, std::wstring& error) = 0;
    virtual void Stop() = 0;

    virtual uint16_t Port() const = 0;  // 0 if the output has none
    virtual int Viewers() const = 0;
    // The viewers that are the Gazeora app (player_identity.h): all of them on outputs only
    // the app speaks (UDP, USB).
    virtual int AppViewers() const { return Viewers(); }
    // True once after a player joined or lost a frame: the next frame should be a key frame.
    virtual bool TakeKeyFrameRequest() = 0;
    // Sends one frame to every player. Loss the players see is reported to `bitrate`.
    virtual void Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
                      uint64_t now) = 0;

    // What a player opens to watch from `host`, e.g. "http://192.168.1.20:8554/stream.ts";
    // empty for outputs players don't open by address (USB).
    virtual std::wstring Url(const std::wstring& host) const = 0;
    // A line on how things stand, for outputs without a URL; empty otherwise.
    virtual std::wstring Status() const { return {}; }
    // Whether a phone is waiting for the USB driver Gazeora installs (USB only).
    virtual bool NeedsUsbDriver() const { return false; }
    // The phones it can't switch without that driver on them (USB only).
    virtual std::vector<UsbPhone> PhonesNeedingDriver() const { return {}; }
    // Where touches the players send on the output's own link go (USB only: the network's go
    // to PointerInputReceiver). Set before Start.
    virtual void SetPointerSink(PointerEventSink* sink) { (void)sink; }
    // Whether a player at `address` is watching: the touches that come over the network
    // (PointerInputReceiver) count only from one.
    virtual bool Watching(const in_addr& address) {
        (void)address;
        return false;
    }
};

// The output for `protocol`; null for the RTSP ones, which the server handles itself.
std::unique_ptr<FrameOutput> CreateFrameOutput(StreamProtocol protocol, StreamLog& log);

}  // namespace gazeora
