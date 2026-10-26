// How the Gazeora app should show the stream, sent with every frame as an SEI message (user
// data unregistered, payload type 5) so it reaches the phone over any protocol and applies
// from the frame it comes with. Other players skip SEI they don't know.
//
// The message is the 16-byte id "GazeoraViewInfo1" followed by text, e.g.
//   vr=1 pair=0 k1=0.340 k2=0.550 scale=0.700 sep=0.500 chroma=0.000
// vr: the phone does the lens correction (VrLayout::lensOnPhone), with these lens settings
// (VrLayout); pair: the frame holds two views side by side, left eye's on the left, each
// squeezed to half the width; src=app, only on frames of Gazeora's own window: the phone
// floats it in the room rather than filling the eyes. A frame without the message is shown as
// it is.
#pragma once

#include <string>

#include "gazeora/host/capture/vr_layout.h"
#include "gazeora/host/encode/video_codec.h"
#include "gazeora/host/stream/frame_source.h"

namespace gazeora {

constexpr char kViewInfoId[] = "GazeoraViewInfo1";  // 16 bytes, no terminator sent

// The message's text for a frame; empty when the phone has nothing to do (no VR, or the PC
// warps), in which case no message is sent.
std::string ViewInfoText(const VrLayout& vr, bool pair, FrameSource source);

// The SEI NAL unit, without a start code, carrying `text` for `codec`.
std::string ViewInfoNal(VideoCodec codec, const std::string& text);

// Whether `nal` (without a start code) is a picture's coded slice, which SEI must come before.
bool IsPictureNal(VideoCodec codec, const std::string& nal);

}  // namespace gazeora
