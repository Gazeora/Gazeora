#include "mappers/launch_via_names.h"

#include <cstring>

namespace gazeora {

namespace {

struct LaunchViaName {
    LaunchVia via;
    const char* name;
};

constexpr LaunchViaName kNames[] = {
    {LaunchVia::Auto, "auto"},
    {LaunchVia::Direct, "direct"},
    {LaunchVia::Steam, "steam"},
};

}  // namespace

const char* LaunchViaNames::NameOf(LaunchVia via) {
    for (const LaunchViaName& entry : kNames) {
        if (entry.via == via) {
            return entry.name;
        }
    }
    return kNames[0].name;
}

LaunchVia LaunchViaNames::FromName(const std::string& name) {
    for (const LaunchViaName& entry : kNames) {
        if (name == entry.name) {
            return entry.via;
        }
    }
    return LaunchVia::Auto;
}

}  // namespace gazeora
