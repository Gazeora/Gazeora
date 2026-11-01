// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/packet_batch.h"

#include <cstring>

namespace gazeora {

void PacketBatch::Assign(const std::vector<std::string>& packets) {
    size_t total = 0;
    for (const std::string& packet : packets) {
        total += packet.size();
    }
    m_bytes.resize(total);
    m_ends.resize(packets.size());
    size_t at = 0;
    for (size_t i = 0; i < packets.size(); ++i) {
        std::memcpy(m_bytes.data() + at, packets[i].data(), packets[i].size());
        at += packets[i].size();
        m_ends[i] = at;
    }
}

}  // namespace gazeora
