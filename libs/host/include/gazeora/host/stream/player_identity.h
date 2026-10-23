#pragma once

#include <string>

namespace gazeora {

// Whether a player is the Gazeora Android app, from the User-Agent it sends with its RTSP and
// HTTP requests. Over UDP and USB only the app speaks Gazeora's own format (GZF1), so every
// player there is the app.
class PlayerIdentity {
public:
    static constexpr char kAppUserAgent[] = "Gazeora Android";

    static bool IsGazeoraApp(const std::string& userAgent) {
        return userAgent.rfind(kAppUserAgent, 0) == 0;
    }
};

}  // namespace gazeora
