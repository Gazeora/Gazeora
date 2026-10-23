#include "gazeora/host/stream/frame_output.h"

#include "gazeora/host/stream/http_ts_output.h"
#include "gazeora/host/stream/udp_frame_output.h"
#include "gazeora/host/stream/usb_accessory_output.h"

namespace gazeora {

std::unique_ptr<FrameOutput> CreateFrameOutput(StreamProtocol protocol, StreamLog& log) {
    switch (protocol) {
        case StreamProtocol::Udp:
            return std::make_unique<UdpFrameOutput>(log);
        case StreamProtocol::Http:
            return std::make_unique<HttpTsOutput>(log);
        case StreamProtocol::Usb:
            return std::make_unique<UsbAccessoryOutput>(log);
        case StreamProtocol::RtspTcp:
        case StreamProtocol::RtspUdp:
            break;
    }
    return nullptr;
}

}  // namespace gazeora
