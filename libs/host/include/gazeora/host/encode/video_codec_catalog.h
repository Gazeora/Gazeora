// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/common/catalog/descriptor_catalog.h"
#include "gazeora/host/encode/video_codec_info.h"

namespace gazeora {

// The codecs this build offers, in the order the settings list them. Whether this PC can
// encode one is the VideoEncoderRegistry's to say.
using VideoCodecCatalog = DescriptorCatalog<VideoCodec, VideoCodecInfo>;

}  // namespace gazeora
