// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/common/catalog/descriptor_catalog.h"
#include "gazeora/host/stream/stream_protocol_info.h"

namespace gazeora {

// The protocols this build offers, in the order the settings list them.
using StreamProtocolCatalog = DescriptorCatalog<StreamProtocol, StreamProtocolInfo>;

}  // namespace gazeora
