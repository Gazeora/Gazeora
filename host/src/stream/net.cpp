#include "gazeora/host/stream/net.h"

#include <iphlpapi.h>
#include <ws2tcpip.h>

namespace gazeora::net {

bool WinsockRuntime::Start(std::wstring& error) {
    if (m_started) {
        return true;
    }
    WSADATA wsa;
    if (int result = WSAStartup(MAKEWORD(2, 2), &wsa)) {
        error = L"WSAStartup failed (error " + std::to_wstring(result) + L")";
        return false;
    }
    m_started = true;
    return true;
}

void WinsockRuntime::Shutdown() {
    if (m_started) {
        WSACleanup();
        m_started = false;
    }
}

void Socket::Reset(SOCKET s) {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
    }
    m_socket = s;
}

bool Socket::SetNonBlocking() {
    u_long nonBlocking = 1;
    return ioctlsocket(m_socket, FIONBIO, &nonBlocking) == 0;
}

bool Socket::SetSendBuffer(int bytes) {
    return setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&bytes),
                      sizeof(bytes)) == 0;
}

Socket BindUdp(in_addr address, uint16_t preferredPort, uint16_t& port) {
    for (uint16_t candidate : {preferredPort, uint16_t(0)}) {
        Socket s(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        if (!s) {
            return s;
        }
        sockaddr_in bound = {};
        bound.sin_family = AF_INET;
        bound.sin_addr = address;
        bound.sin_port = htons(candidate);
        int length = sizeof(bound);
        if (bind(s.Get(), reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) == 0 &&
            getsockname(s.Get(), reinterpret_cast<sockaddr*>(&bound), &length) == 0) {
            port = ntohs(bound.sin_port);
            return s;
        }
    }
    return Socket();
}

std::string AddressText(const sockaddr_in& address) {
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

std::vector<std::wstring> LanAddresses() {
    ULONG size = 16384;
    std::vector<uint8_t> buffer;
    ULONG result;
    do {
        buffer.resize(size);
        result = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    } while (result == ERROR_BUFFER_OVERFLOW);
    std::vector<std::wstring> withGateway, others;
    if (result != NO_ERROR) {
        return others;
    }
    for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            auto* address = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
            wchar_t text[INET_ADDRSTRLEN];
            if (InetNtopW(AF_INET, &address->sin_addr, text, INET_ADDRSTRLEN)) {
                (adapter->FirstGatewayAddress ? withGateway : others).push_back(text);
            }
        }
    }
    return withGateway.empty() ? others : withGateway;
}

}  // namespace gazeora::net
