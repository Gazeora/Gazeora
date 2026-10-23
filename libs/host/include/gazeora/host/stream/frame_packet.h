#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gazeora {

// The "GZF1" wire format of the UDP and USB protocols: encoded frames with a small header,
// no RTSP or RTP. The Android app has its own copy (FramePacket.java); keep the two in step.
//
// Every message starts with a 24-byte header, big-endian:
//    0  "GZF1"
//    4  u8   type (FrameMessage)
//    5  u8   flags: bit 0 key frame, bit 1 H.265 (else H.264)
//    6  u16  width
//    8  u16  height
//   10  u16  in a hello: the share of frames the player lost lately, per mille; else 0
//   12  u32  frame number, counting up from the first frame sent
//   16  u32  timestamp, 90 kHz
//   20  u32  UDP: chunk index (high 16 bits) and chunk count (low 16 bits)
//            USB: payload length
// A frame's payload is its access unit in Annex B, the parameter sets in front of a key
// frame. Over UDP it's cut into chunks, one per datagram of at most kMaxFrameDatagram bytes,
// sent in order. Over USB it follows the header whole.
//
// The player says hello once a second (UDP: which is how the PC knows where to send; a
// player silent for kPlayerTimeoutMs is dropped), asks for a key frame when it starts or
// lost a frame, and says bye when it stops.
enum class FrameMessage : uint8_t {
    Frame = 1,            // PC to player
    Hello = 2,            // player to PC
    KeyFrameRequest = 3,  // player to PC
    Bye = 4,              // player to PC
};

constexpr size_t kFrameHeaderSize = 24;
constexpr size_t kMaxFrameDatagram = 1400;  // under a typical 1500-byte MTU
constexpr uint64_t kPlayerTimeoutMs = 5000;
constexpr uint8_t kFrameFlagKey = 1;
constexpr uint8_t kFrameFlagH265 = 2;

struct FrameHeader {
    FrameMessage type = FrameMessage::Frame;
    uint8_t flags = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t lossPerMille = 0;
    uint32_t frameNumber = 0;
    uint32_t timestamp = 0;
    uint32_t tail = 0;  // UDP: chunk index << 16 | chunk count. USB: payload length.

    void Write(uint8_t out[kFrameHeaderSize]) const;
    // False if `data` is too short or not GZF1.
    bool Read(const uint8_t* data, size_t size);
};

// One frame as UDP datagrams: `header` (a Frame) with the chunk fields filled in, each
// followed by its piece of `annexB`.
void ChunkFrame(FrameHeader header, const std::string& annexB, std::vector<std::string>& datagrams);
// One message for a byte stream (USB): the header with the payload length, then `payload`.
std::string StreamMessage(FrameHeader header, const std::string& payload);

}  // namespace gazeora
