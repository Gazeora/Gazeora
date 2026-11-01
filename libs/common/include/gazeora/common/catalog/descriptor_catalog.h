// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

namespace gazeora {

// Registry pattern: descriptors of things known by an id (a protocol, a codec...), in the
// order they were registered. `Descriptor` has a `Key id` and a `std::string name`; `Key` has
// Id() and ==. Fill it before handing it out; it's read only after that.
template <typename Key, typename Descriptor>
class DescriptorCatalog final {
public:
    // Replaces the descriptor with the same id, keeping its place; otherwise added last.
    void Register(Descriptor descriptor) {
        for (Descriptor& known : m_descriptors) {
            if (known.id == descriptor.id) {
                known = std::move(descriptor);
                return;
            }
        }
        m_descriptors.push_back(std::move(descriptor));
    }

    const std::vector<Descriptor>& All() const { return m_descriptors; }
    // Null if there's none for `key`.
    const Descriptor* Find(const Key& key) const {
        for (const Descriptor& descriptor : m_descriptors) {
            if (descriptor.id == key) {
                return &descriptor;
            }
        }
        return nullptr;
    }
    bool Has(const Key& key) const { return Find(key) != nullptr; }
    // Its name for the UI and logs; its id when it isn't known.
    std::string NameOf(const Key& key) const {
        const Descriptor* descriptor = Find(key);
        return descriptor ? descriptor->name : key.Id();
    }

private:
    std::vector<Descriptor> m_descriptors;
};

}  // namespace gazeora
