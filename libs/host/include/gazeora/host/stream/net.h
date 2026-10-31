// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <winsock2.h>  // includes windows.h

#include <cstdint>
#include <string>
#include <vector>

namespace gazeora::net {

// Winsock for as long as the object lives (WSAStartup / WSACleanup).
class WinsockRuntime {
public:
    WinsockRuntime() = default;
    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
    ~WinsockRuntime() { Shutdown(); }

    // False with `error` set if Winsock can't start. Idempotent.
    bool Start(std::wstring& error);
    void Shutdown();

private:
    bool m_started = false;
};

// Owns a socket and closes it on destruction. Move-only.
class Socket {
public:
    Socket() = default;
    explicit Socket(SOCKET s) : m_socket(s) {}
    Socket(Socket&& other) noexcept : m_socket(other.Release()) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) Reset(other.Release());
        return *this;
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ~Socket() { Reset(); }

    SOCKET Get() const { return m_socket; }
    bool Valid() const { return m_socket != INVALID_SOCKET; }
    explicit operator bool() const { return Valid(); }

    SOCKET Release() {
        SOCKET s = m_socket;
        m_socket = INVALID_SOCKET;
        return s;
    }
    void Reset(SOCKET s = INVALID_SOCKET);

    bool SetNonBlocking();
    bool SetSendBuffer(int bytes);

private:
    SOCKET m_socket = INVALID_SOCKET;
};

// A UDP socket bound to `address`, on `preferredPort` if it's free and any port otherwise.
// `port` gets the one bound. Invalid if neither works.
Socket BindUdp(in_addr address, uint16_t preferredPort, uint16_t& port);

// "a.b.c.d:port", for logs.
std::string AddressText(const sockaddr_in& address);

// IPv4 addresses other devices can reach this PC on. Prefers adapters with a default
// gateway, which leaves out virtual switches (Hyper-V, WSL) when there's a real network.
std::vector<std::wstring> LanAddresses();

}  // namespace gazeora::net
