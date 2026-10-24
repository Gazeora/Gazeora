#include "gazeora/host/stream/http_ts_output.h"

#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>

#include "gazeora/host/stream/player_identity.h"

namespace gazeora {

namespace {

// A frame is skipped while this much is still queued for the player: about half a second at
// the full bitrate. The socket's own buffer is kept small, so what's queued can be counted.
constexpr size_t kBacklogBytes = 512 * 1024;
constexpr int kSocketSendBufferBytes = 128 * 1024;
constexpr size_t kMaxRequestBytes = 8192;
constexpr uint64_t kRequestTimeoutMs = 10000;
// Connections open at once, each with its thread; more are closed as they come.
constexpr size_t kMaxConnections = 16;

std::string Response(const char* status, const char* type, const std::string& body) {
    return std::string("HTTP/1.1 ") + status + "\r\nServer: Gazeora\r\nContent-Type: " + type +
           "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" +
           body;
}

// The request's User-Agent header, trimmed; empty if it has none.
std::string UserAgent(const std::string& request) {
    constexpr char kHeader[] = "\r\nuser-agent:";
    std::string lower(request.size(), ' ');
    std::transform(request.begin(), request.end(), lower.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    size_t at = lower.find(kHeader);
    if (at == std::string::npos) {
        return {};
    }
    size_t begin = request.find_first_not_of(' ', at + sizeof(kHeader) - 1);
    size_t end = request.find("\r\n", begin);
    return request.substr(begin, end - begin);
}

}  // namespace

bool HttpTsOutput::Start(in_addr address, uint16_t port, std::wstring& error) {
    m_listen.Reset(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!m_listen) {
        error = L"socket failed (error " + std::to_wstring(WSAGetLastError()) + L")";
        return false;
    }
    BOOL exclusive = TRUE;
    setsockopt(m_listen.Get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char*>(&exclusive),
               sizeof(exclusive));
    sockaddr_in bound = {};
    bound.sin_family = AF_INET;
    bound.sin_addr = address;
    bound.sin_port = htons(port);
    int length = sizeof(bound);
    if (bind(m_listen.Get(), reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) == SOCKET_ERROR ||
        listen(m_listen.Get(), SOMAXCONN) == SOCKET_ERROR ||
        getsockname(m_listen.Get(), reinterpret_cast<sockaddr*>(&bound), &length) == SOCKET_ERROR) {
        int code = WSAGetLastError();
        error = code == WSAEADDRINUSE ? L"port " + std::to_wstring(port) + L" is already in use"
                                      : L"cannot listen on port " + std::to_wstring(port) +
                                            L" (error " + std::to_wstring(code) + L")";
        m_listen.Reset();
        return false;
    }
    m_stopping = false;
    m_port = ntohs(bound.sin_port);
    m_log.Write("HTTP stream on port %u", unsigned(m_port));
    m_acceptThread = std::thread([this] { AcceptLoop(); });
    return true;
}

void HttpTsOutput::Stop() {
    m_stopping = true;
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    Reap(true);
    m_listen.Reset();
    m_port = 0;
    m_viewers = 0;
    m_appViewers = 0;
}

std::wstring HttpTsOutput::Url(const std::wstring& host) const {
    return L"http://" + host + L":" + std::to_wstring(m_port) + L"/stream.ts";
}

void HttpTsOutput::AcceptLoop() {
    while (!m_stopping) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_listen.Get(), &readable);
        timeval timeout = {0, 250 * 1000};
        if (select(0, &readable, nullptr, nullptr, &timeout) != 1) {
            Reap(false);
            continue;
        }
        sockaddr_in peer = {};
        int peerLength = sizeof(peer);
        net::Socket client(accept(m_listen.Get(), reinterpret_cast<sockaddr*>(&peer), &peerLength));
        if (!client) {
            continue;
        }
        Reap(false);
        {
            std::lock_guard<std::mutex> lock(m_playersMutex);
            if (m_players.size() >= kMaxConnections) {
                m_log.Write("%s turned away: %u connections are open already",
                            net::AddressText(peer).c_str(), unsigned(m_players.size()));
                continue;  // closes it
            }
        }
        BOOL noDelay = TRUE;
        setsockopt(client.Get(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&noDelay),
                   sizeof(noDelay));
        client.SetSendBuffer(kSocketSendBufferBytes);
        client.SetNonBlocking();

        auto connection =
            std::make_unique<RtspConnection>(std::move(client), net::AddressText(peer));
        m_log.Write("%s connected over HTTP", connection->Peer().c_str());
        RtspConnection* raw = connection.get();
        std::lock_guard<std::mutex> lock(m_playersMutex);
        m_players.push_back(Player{std::move(connection), peer.sin_addr});
        raw->thread = std::thread([this, raw] { Serve(*raw); });
    }
}

void HttpTsOutput::Reap(bool all) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    for (auto it = m_players.begin(); it != m_players.end();) {
        RtspConnection& connection = *it->connection;
        // With `all`, Stop has set m_stopping, which the connection notices within a poll.
        if (all || connection.done) {
            if (connection.thread.joinable()) {
                connection.thread.join();
            }
            it = m_players.erase(it);  // closes the socket
        } else {
            ++it;
        }
    }
}

bool HttpTsOutput::Watching(const in_addr& address) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    return std::any_of(m_players.begin(), m_players.end(), [&](const Player& player) {
        return player.streaming && player.address.s_addr == address.s_addr;
    });
}

void HttpTsOutput::SetStreaming(RtspConnection& connection, bool streaming, bool app) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    for (Player& player : m_players) {
        if (player.connection.get() == &connection) {
            player.streaming = streaming;
            player.app = streaming && app;
            player.waitingForKeyFrame = true;
            player.countedSinceMs = GetTickCount64();
        }
    }
    m_viewers = int(std::count_if(m_players.begin(), m_players.end(),
                                  [](const Player& player) { return player.streaming; }));
    m_appViewers = int(std::count_if(m_players.begin(), m_players.end(),
                                     [](const Player& player) { return player.app; }));
    if (streaming) {
        m_keyFrameWanted = true;  // so it doesn't wait up to a second for a picture
    }
}

bool HttpTsOutput::Answer(RtspConnection& connection, const std::string& request) {
    size_t lineEnd = request.find("\r\n");
    std::string line = request.substr(0, lineEnd);
    size_t space = line.find(' ');
    size_t space2 = line.find(' ', space == std::string::npos ? space : space + 1);
    std::string method = line.substr(0, space);
    std::string path = space == std::string::npos ? "" : line.substr(space + 1, space2 - space - 1);
    path = path.substr(0, path.find('?'));
    m_log.Write("%s: %s", connection.Peer().c_str(), line.c_str());

    if (method != "GET" && method != "HEAD") {
        connection.Send(Response("405 Method Not Allowed", "text/plain", "GET only\n"));
        return false;
    }
    if (path != "/" && path != "/stream.ts") {
        connection.Send(Response("404 Not Found", "text/plain", "The stream is at /stream.ts\n"));
        return false;
    }
    // No length: the stream lasts until the connection closes. No Access-Control-Allow-Origin:
    // players don't need it, and with it any web page open on this PC or the network could read
    // the stream.
    connection.Send(
        "HTTP/1.1 200 OK\r\nServer: Gazeora\r\nContent-Type: video/mp2t\r\n"
        "Cache-Control: no-cache, no-store\r\nConnection: close\r\n\r\n");
    return method == "GET";
}

void HttpTsOutput::Serve(RtspConnection& connection) {
    const SOCKET socket = connection.Socket();
    std::string request;
    char chunk[4096];
    const uint64_t connectedMs = GetTickCount64();
    bool streaming = false;
    const char* reason = "the server stopped";
    while (!m_stopping) {
        bool pending = connection.HasPending();
        fd_set readable, writable;
        FD_ZERO(&readable);
        FD_SET(socket, &readable);
        FD_ZERO(&writable);
        FD_SET(socket, &writable);
        timeval timeout = {0, (streaming ? 5 : 250) * 1000};
        int ready = select(0, &readable, pending ? &writable : nullptr, nullptr, &timeout);
        if (ready < 0) {
            reason = "the connection failed";
            break;
        }
        if (pending && FD_ISSET(socket, &writable) && !connection.Flush()) {
            reason = "sending to it failed";
            break;
        }
        if (!streaming && !pending && request.find("\r\n\r\n") != std::string::npos) {
            reason = "it was answered";  // an error, or HEAD: all sent
            break;
        }
        if (!FD_ISSET(socket, &readable)) {
            if (!streaming && GetTickCount64() - connectedMs > kRequestTimeoutMs) {
                reason = "it sent no request";
                break;
            }
            continue;
        }
        int got = recv(socket, chunk, sizeof(chunk), 0);
        if (got == 0) {
            reason = "it closed the connection";
            break;
        }
        if (got < 0) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                continue;
            }
            reason = "the connection broke";
            break;
        }
        if (streaming) {
            continue;  // nothing more is expected from a player
        }
        request.append(chunk, size_t(got));
        if (request.find("\r\n\r\n") != std::string::npos) {
            streaming = Answer(connection, request);
            if (streaming) {
                SetStreaming(connection, true, PlayerIdentity::IsGazeoraApp(UserAgent(request)));
            }
        } else if (request.size() > kMaxRequestBytes) {
            reason = "its request was too large";
            break;
        }
    }
    if (streaming) {
        SetStreaming(connection, false);
    }
    m_log.Write("%s disconnected after %llu s: %s", connection.Peer().c_str(),
                (unsigned long long)((GetTickCount64() - connectedMs) / 1000), reason);
    shutdown(socket, SD_SEND);
    connection.done = true;
}

void HttpTsOutput::Send(const VideoPipeline::EncodedFrame& frame, BitrateController& bitrate,
                        uint64_t now) {
    m_ts.clear();
    // Muxed even with no one watching, so the continuity counters run on.
    m_muxer.Mux(frame.codec, frame.annexB, frame.timestamp, frame.keyFrame, m_ts);
    std::lock_guard<std::mutex> lock(m_playersMutex);
    for (Player& player : m_players) {
        if (!player.streaming) {
            continue;
        }
        RtspConnection& connection = *player.connection;
        bool full = connection.Backlog() > kBacklogBytes;
        if (full || (player.waitingForKeyFrame && !frame.keyFrame)) {
            if (full) {
                player.waitingForKeyFrame = true;
                ++player.framesSkipped;
            } else {
                m_keyFrameWanted = true;
            }
        } else {
            player.waitingForKeyFrame = false;
            ++player.framesSent;
            connection.Send(m_ts);
        }
        if (now - player.countedSinceMs >= 1000) {
            uint32_t frames = player.framesSent + player.framesSkipped;
            float loss = frames ? float(player.framesSkipped) / float(frames) : 0.0f;
            bitrate.Report(uint64_t(uintptr_t(&connection)), loss, now);
            if (player.framesSkipped > 0) {
                m_log.Write("%s skipped %u of %u frames: the connection can't keep up",
                            connection.Peer().c_str(), player.framesSkipped, frames);
            }
            player.framesSent = player.framesSkipped = 0;
            player.countedSinceMs = now;
        }
    }
}

}  // namespace gazeora
