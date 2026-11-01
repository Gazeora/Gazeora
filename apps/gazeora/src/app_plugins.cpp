// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// App: plugins installed and uninstalled while it runs, and the stream restarts they need.
#include "app.h"

#include "ui/text_format.h"

namespace gazeora::app {

void App::OnPluginChanged(const PluginStatus& plugin, bool added) {
    m_libraryServices.SetStores(m_plugins.Stores());
    if (const Game* game = m_editLaunchVia ? m_library.Find(m_editId) : nullptr) {
        m_editLaunchVia.emplace(m_libraryServices.Stores(), *game);  // its store list changed
    }
    m_gamepadControl.SetDriver(m_plugins.GamepadDrivers(), m_plugins.GamepadSwitch());

    m_stream.SetParts(
        {m_plugins.Encoders(), m_plugins.Outputs(), m_plugins.Protocols(), m_plugins.Codecs()});
    // As at start: the saved protocol if a plugin now offers it, RTSP over TCP if its plugin
    // is gone.
    const StreamProtocol protocol = SavedProtocol();
    const bool protocolChanged = protocol != m_stream.Protocol();
    m_stream.SetProtocol(protocol);
    // The running stream keeps the encoders and codecs it started with.
    if (protocolChanged || plugin.encoders > 0 || plugin.codecs > 0) {
        RestartStream();
    }

    if (!added) {
        ShowToast(U8(plugin.name) + " is uninstalled");
    } else if (plugin.loaded) {
        ShowToast(U8(plugin.name) + " is installed and ready");
    } else {
        ShowToast(U8(plugin.name) + " didn't load: " + U8(plugin.error), true);
    }
}

void App::RestartStream() {
    const bool watched = m_stream.Viewers() > 0;
    m_stream.Stop();
    StartStream();
    if (watched && m_stream.Running()) {
        ShowToast("The stream restarted for the plugin. Viewers need to reconnect.");
    }
}

StreamProtocol App::SavedProtocol() {
    // A protocol no plugin offers (saved with one that's gone, or by another version) falls back
    // to the default.
    const StreamProtocol protocol = m_settings.LoadProtocol();
    return m_plugins.Protocols()->Has(protocol) ? protocol : StreamProtocol::RtspTcp;
}

}  // namespace gazeora::app
