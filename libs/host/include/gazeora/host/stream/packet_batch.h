// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace gazeora {

// A frame's packets back to back in one buffer, with where each ends: one allocation for the
// whole frame instead of one per packet, read in order by the thread that sends them. Assign
// keeps the buffers' capacity, so a reused batch doesn't allocate once it has grown.
class PacketBatch {
public:
    void Assign(const std::vector<std::string>& packets);

    size_t Count() const { return m_ends.size(); }
    std::string_view Packet(size_t index) const {
        const size_t begin = index == 0 ? 0 : m_ends[index - 1];
        return std::string_view(m_bytes.data() + begin, m_ends[index] - begin);
    }

private:
    std::vector<char> m_bytes;
    std::vector<size_t> m_ends;
};

}  // namespace gazeora
