// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/bundled_parts/bundled_protocols.h"

#include "gazeora/bundled/http/http_ts_output.h"
#include "gazeora/bundled/udp/udp_frame_output.h"
#include "gazeora/bundled/usb/usb_accessory_output.h"
#include "gazeora/host/stream/built_in_stream_protocols.h"
#include "gazeora/host/stream/simple_frame_output_factory.h"

namespace gazeora {

namespace {

template <typename Output>
void RegisterOne(StreamProtocolCatalog& catalog, FrameOutputRegistry& outputs) {
    StreamProtocolInfo info = Output::Info();
    outputs.Register(std::make_shared<SimpleFrameOutputFactory<Output>>(info.id));
    catalog.Register(std::move(info));
}

}  // namespace

void BundledProtocols::RegisterInto(StreamProtocolCatalog& catalog, FrameOutputRegistry& outputs) {
    RegisterOne<UdpFrameOutput>(catalog, outputs);
    RegisterOne<HttpTsOutput>(catalog, outputs);
    RegisterOne<UsbAccessoryOutput>(catalog, outputs);
}

std::shared_ptr<const StreamProtocolCatalog> BundledProtocols::Catalog() {
    auto catalog = std::make_shared<StreamProtocolCatalog>();
    BuiltInStreamProtocols::RegisterInto(*catalog);
    FrameOutputRegistry unused;
    RegisterInto(*catalog, unused);
    return catalog;
}

std::shared_ptr<const FrameOutputRegistry> BundledProtocols::Outputs() {
    auto outputs = std::make_shared<FrameOutputRegistry>();
    StreamProtocolCatalog unused;
    RegisterInto(unused, *outputs);
    return outputs;
}

}  // namespace gazeora
