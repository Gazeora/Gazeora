# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# USB Cable: streams to a phone in USB accessory mode (UsbAccessoryOutput), through libusb.
include(LibUsb)

add_library(gazeora_usb STATIC src/usb_accessory_output.cpp)
target_include_directories(gazeora_usb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_usb PUBLIC gazeora_host_lib PRIVATE libusb setupapi winusb)

gazeora_add_bundled_plugin(gazeora_usb_plugin
    ID gazeora-usb CATEGORY protocol NAME "USB Cable"
    SUMMARY "Streams to the Gazeora app on a phone plugged into this PC: no Wi-Fi needed."
    SOURCES src/usb_plugin.cpp
    LIBS gazeora_usb)
