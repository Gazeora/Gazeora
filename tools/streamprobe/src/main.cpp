// Minimal RTSP player for measuring Gazeora's frame stream without VLC: plays an rtsp://
// URL over UDP the way VLC does, rebuilds every H.264 or H.265 frame, and reports what
// arrived, what was damaged by loss, and when.
//
//   gazeora_stream_probe.exe [rtsp://host:port/] [--seconds N] [--save stream.h264]
//                            [--drop PERCENT] [--tcp]
//
// --tcp asks for RTP interleaved in the RTSP connection, like VLC's "RTSP over TCP".
//
// --save writes everything received as a raw H.264 or H.265 stream, which VLC and ffplay
// play, to see what loss does to the picture.
//
// Like VLC, it sends RTCP receiver reports, so the server sees the loss and adapts to it.
// --drop throws away that share of arriving packets, to try a bad Wi-Fi link on a good one.
//
// Latency is capture to arrival: each frame's RTP timestamp, mapped to the sender's clock
// through its RTCP sender reports, against this machine's clock. Exact when run on the
// streaming PC; from another machine it is off by however far apart the two clocks are.

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gazeora/host/stream/rtp_video.h"

namespace {

constexpr double kMs1601To1900 = 9435484800.0 * 1000.0;
constexpr DWORD kKeepAliveMs = 25000;  // the server drops sessions silent for 60 s

// Wall-clock time in ms since 1900, the NTP epoch the sender reports use.
double NowNtpMs() {
    FILETIME now;
    GetSystemTimePreciseAsFileTime(&now);
    return double(uint64_t(now.dwHighDateTime) << 32 | now.dwLowDateTime) / 10000.0 - kMs1601To1900;
}

double NowMs() {
    static const double freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return double(f.QuadPart);
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return double(now.QuadPart) * 1000.0 / freq;
}

uint32_t Get16(const uint8_t* p) { return uint32_t(p[0]) << 8 | p[1]; }
uint32_t Get32(const uint8_t* p) { return Get16(p) << 16 | Get16(p + 2); }

// An RTCP receiver report with one block (RFC 3550 6.4.2), as players send every few seconds.
std::string ReceiverReport(uint32_t ssrc, uint32_t source, uint8_t fractionLost,
                           uint32_t cumulativeLost, uint32_t highestSeq) {
    std::string report;
    auto put32 = [&](uint32_t v) {
        for (int shift = 24; shift >= 0; shift -= 8) report += char(v >> shift);
    };
    put32(0x81C90007);  // V=2, one block, RR, 8 words
    put32(ssrc);
    put32(source);
    put32(uint32_t(fractionLost) << 24 | (cumulativeLost & 0xFFFFFF));
    put32(highestSeq);
    put32(0);  // jitter
    put32(0);  // last SR and the delay since: not used by Gazeora
    put32(0);
    return report;
}

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(tolower(c)); });
    return text;
}

// The value of header `name` in an RTSP response head, matched case-insensitively.
std::string Header(const std::string& head, const std::string& name) {
    std::string lower = Lower(head);
    size_t at = lower.find("\r\n" + Lower(name) + ":");
    if (at == std::string::npos) return {};
    at += name.size() + 3;
    size_t end = head.find("\r\n", at);
    std::string value = head.substr(at, end - at);
    return value.substr(value.find_first_not_of(' '));
}

struct Rtsp {
    SOCKET socket = INVALID_SOCKET;
    int cseq = 0;
    std::string received;  // read but not yet taken: responses and interleaved packets

    void Send(const std::string& method, const std::string& url, const std::string& headers) {
        std::string request = method + " " + url + " RTSP/1.0\r\nCSeq: " + std::to_string(++cseq) +
                              "\r\nUser-Agent: Gazeora stream probe\r\n" + headers + "\r\n";
        send(socket, request.data(), int(request.size()), 0);
    }

    // Sends a request and returns the response head; the body goes to `body`. Only before
    // interleaved data starts flowing.
    std::string Request(const std::string& method, const std::string& url,
                        const std::string& headers = {}, std::string* body = nullptr) {
        Send(method, url, headers);
        std::string head;
        while (!TakeResponse(&head, body)) {
            if (!ReceiveMore()) return {};
        }
        return head;
    }

    bool ReceiveMore() {
        char buffer[65536];
        int got = recv(socket, buffer, sizeof(buffer), 0);
        if (got <= 0) return false;
        received.append(buffer, size_t(got));
        return true;
    }

    // Takes a whole response from the front of `received`, if there is one.
    bool TakeResponse(std::string* head, std::string* body) {
        size_t headEnd = received.find("\r\n\r\n");
        if (received.rfind("RTSP/", 0) != 0 || headEnd == std::string::npos) return false;
        std::string text = received.substr(0, headEnd + 2);
        size_t length = strtoul(Header(text, "Content-Length").c_str(), nullptr, 10);
        if (received.size() < headEnd + 4 + length) return false;
        if (head) *head = text;
        if (body) *body = received.substr(headEnd + 4, length);
        received.erase(0, headEnd + 4 + length);
        return true;
    }

    // Takes an interleaved packet from the front of `received`, skipping responses (to
    // keepalives). False when none is complete yet.
    bool TakeInterleaved(uint8_t& channel, std::string& packet) {
        while (TakeResponse(nullptr, nullptr)) {
        }
        if (received.size() < 4 || received[0] != '$') return false;
        size_t size = uint8_t(received[2]) << 8 | uint8_t(received[3]);
        if (received.size() < 4 + size) return false;
        channel = uint8_t(received[1]);
        packet = received.substr(4, size);
        received.erase(0, 4 + size);
        return true;
    }
};

bool Ok(const std::string& head) { return head.rfind("RTSP/1.0 200", 0) == 0; }

std::string StatusLine(const std::string& head) {
    return head.empty() ? "no response" : head.substr(0, head.find("\r\n"));
}

double Percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, size_t(p * double(values.size())))];
}

// Per-second figures, printed and reset each second.
struct Interval {
    int frames = 0;
    size_t bytes = 0;
    uint32_t packets = 0;
    uint32_t lost = 0;
    double latencySum = 0, latencyMax = 0;
    int latencyCount = 0;
    int damaged = 0;
    int keyFrames = 0;
    double gapMax = 0;
};

int Usage() {
    fprintf(stderr,
            "usage: gazeora_stream_probe [rtsp://host:port/] [--seconds N] [--save stream.h264]\n"
            "                            [--drop PERCENT] [--tcp]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string url = "rtsp://127.0.0.1:8554/";
    int seconds = 10;
    const char* savePath = nullptr;
    double dropPercent = 0;
    bool tcp = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--save") && i + 1 < argc) {
            savePath = argv[++i];
        } else if (!strcmp(argv[i], "--drop") && i + 1 < argc) {
            dropPercent = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--tcp")) {
            tcp = true;
        } else if (!strncmp(argv[i], "rtsp://", 7)) {
            url = argv[i];
        } else {
            return Usage();
        }
    }
    if (seconds <= 0) return Usage();

    // rtsp://host[:port][/path]
    size_t hostStart = 7, pathStart = url.find('/', hostStart);
    std::string authority = url.substr(hostStart, pathStart - hostStart);
    size_t colon = authority.find(':');
    std::string host = authority.substr(0, colon);
    std::string port = colon == std::string::npos ? "554" : authority.substr(colon + 1);
    if (pathStart == std::string::npos) url += "/";

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    addrinfo hints = {}, *address = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &address) != 0) {
        fprintf(stderr, "cannot resolve %s\n", host.c_str());
        return 1;
    }
    Rtsp rtsp;
    rtsp.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    DWORD timeoutMs = 5000;
    setsockopt(rtsp.socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeoutMs),
               sizeof(timeoutMs));
    if (connect(rtsp.socket, address->ai_addr, int(address->ai_addrlen)) != 0) {
        fprintf(stderr, "cannot connect to %s:%s (error %d)\n", host.c_str(), port.c_str(),
                WSAGetLastError());
        return 1;
    }
    sockaddr_in server = *reinterpret_cast<sockaddr_in*>(address->ai_addr);
    freeaddrinfo(address);

    // RTP on one port, RTCP on the next.
    SOCKET rtp = INVALID_SOCKET, rtcp = INVALID_SOCKET;
    uint16_t rtpPort = 0;
    for (int attempt = 0; attempt < 20 && rtcp == INVALID_SOCKET; ++attempt) {
        if (rtp != INVALID_SOCKET) closesocket(rtp);
        rtp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in local = {};
        local.sin_family = AF_INET;
        int length = sizeof(local);
        bind(rtp, reinterpret_cast<sockaddr*>(&local), sizeof(local));
        getsockname(rtp, reinterpret_cast<sockaddr*>(&local), &length);
        rtpPort = ntohs(local.sin_port);
        rtcp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        local.sin_port = htons(uint16_t(rtpPort + 1));
        if (bind(rtcp, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
            closesocket(rtcp);
            rtcp = INVALID_SOCKET;
        }
    }
    int receiveBuffer = 8 << 20;
    setsockopt(rtp, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&receiveBuffer),
               sizeof(receiveBuffer));

    std::string sdp;
    std::string head = rtsp.Request("DESCRIBE", url, "Accept: application/sdp\r\n", &sdp);
    size_t media = sdp.find("m=video ");
    size_t rtpmap = sdp.find(" H264/90000", media);
    gazeora::VideoCodec codec = gazeora::VideoCodec::H264;
    if (rtpmap == std::string::npos && media != std::string::npos) {
        rtpmap = sdp.find(" H265/90000", media);
        codec = gazeora::VideoCodec::H265;
    }
    if (!Ok(head) || media == std::string::npos || rtpmap == std::string::npos) {
        fprintf(stderr, "DESCRIBE: %s, no H.264 or H.265 video track\n", StatusLine(head).c_str());
        return 1;
    }
    // The dynamic payload type, from "a=rtpmap:96 H264/90000" (or H265).
    int payloadType = atoi(sdp.c_str() + sdp.rfind(':', rtpmap) + 1);
    std::string base = Header(head, "Content-Base");
    if (base.empty()) base = url;
    std::string track = base;
    size_t control = sdp.find("a=control:", media);
    if (control != std::string::npos) {
        std::string value = sdp.substr(control + 10, sdp.find("\r\n", control) - control - 10);
        track = value.rfind("rtsp://", 0) == 0 ? value : base + value;
    }
    head = rtsp.Request("SETUP", track,
                        tcp ? "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                            : "Transport: RTP/AVP;unicast;client_port=" + std::to_string(rtpPort) +
                                  "-" + std::to_string(rtpPort + 1) + "\r\n");
    if (!Ok(head)) {
        fprintf(stderr, "SETUP: %s\n", StatusLine(head).c_str());
        return 1;
    }
    std::string session = Header(head, "Session");
    session = session.substr(0, session.find(';'));
    // Receiver reports go to the second of the server's ports.
    std::string transport = Header(head, "Transport");
    size_t serverPorts = transport.find("server_port=");
    sockaddr_in serverRtcp = server;
    if (serverPorts != std::string::npos) {
        char* rangeEnd = nullptr;
        unsigned long first = strtoul(transport.c_str() + serverPorts + 12, &rangeEnd, 10);
        unsigned long second = *rangeEnd == '-' ? strtoul(rangeEnd + 1, nullptr, 10) : first + 1;
        serverRtcp.sin_port = htons(uint16_t(second));
    }
    head = rtsp.Request("PLAY", base, "Session: " + session + "\r\nRange: npt=0.000-\r\n");
    if (!Ok(head)) {
        fprintf(stderr, "PLAY: %s\n", StatusLine(head).c_str());
        return 1;
    }
    if (tcp) {
        printf("playing %s for %d s (RTP interleaved over TCP)\n", url.c_str(), seconds);
    } else {
        printf("playing %s for %d s (RTP/UDP on port %u)\n", url.c_str(), seconds, rtpPort);
    }
    printf("codec %s\n", gazeora::CodecName(codec));
    if (dropPercent > 0) printf("dropping %.1f%% of arriving packets\n", dropPercent);
    printf("\n");
    printf("  time     fps    Mbit/s   loss   damaged   keys   latency avg/max   gap max\n");

    // Sender clock, from the latest RTCP sender report.
    bool haveReport = false;
    double reportNtpMs = 0;
    uint32_t reportRtp = 0;
    // RTP sequence tracking.
    bool haveSeq = false;
    uint16_t nextSeq = 0;
    uint32_t packetsTotal = 0, lostTotal = 0;
    uint32_t sourceSsrc = 0, receiverSsrc = GetTickCount() * 2654435761u;
    // The frame being assembled: everything with one RTP timestamp, up to the marker bit.
    gazeora::RtpVideoDepacketizer depacketizer(codec);
    std::string frame;
    bool assembling = false, intact = false;
    uint32_t frameTimestamp = 0;
    std::vector<std::string> nalUnits;
    // Totals.
    int framesTotal = 0, damagedTotal = 0, keyFramesTotal = 0;
    size_t bytesTotal = 0;
    std::vector<double> latencies, gaps;
    double lastFrameMs = 0;
    FILE* saveFile = savePath ? fopen(savePath, "wb") : nullptr;

    Interval interval;
    double start = NowMs(), intervalStart = start, lastKeepAlive = start;
    uint8_t packet[65536];
    while (NowMs() - start < seconds * 1000.0) {
        double now = NowMs();
        if (now - intervalStart >= 1000.0) {
            double span = (now - intervalStart) / 1000.0;
            uint32_t expected = interval.packets + interval.lost;
            char latency[32] = "-";
            if (interval.latencyCount) {
                snprintf(latency, sizeof(latency), "%.0f / %.0f ms",
                         interval.latencySum / interval.latencyCount, interval.latencyMax);
            }
            printf("  %4.0f s  %5.1f  %7.2f  %5.1f%%  %7d  %5d   %-17s %5.0f ms\n",
                   (now - start) / 1000.0, interval.frames / span,
                   interval.bytes * 8.0 / span / 1e6,
                   expected ? 100.0 * interval.lost / expected : 0.0, interval.damaged,
                   interval.keyFrames, latency, interval.gapMax);
            fflush(stdout);
            if (sourceSsrc && serverRtcp.sin_port && !tcp) {
                uint8_t fraction =
                    uint8_t(std::min(255u, expected ? interval.lost * 256 / expected : 0u));
                std::string report =
                    ReceiverReport(receiverSsrc, sourceSsrc, fraction, lostTotal, nextSeq);
                sendto(rtcp, report.data(), int(report.size()), 0,
                       reinterpret_cast<sockaddr*>(&serverRtcp), sizeof(serverRtcp));
            }
            interval = Interval();
            intervalStart = now;
        }
        if (now - lastKeepAlive >= kKeepAliveMs) {
            if (tcp) {
                rtsp.Send("GET_PARAMETER", base, "Session: " + session + "\r\n");  // answer skipped
            } else {
                rtsp.Request("GET_PARAMETER", base, "Session: " + session + "\r\n");
            }
            lastKeepAlive = now;
        }

        // Either way, one packet into `packet`: `got` bytes, RTCP if `isRtcp`.
        int got = 0;
        bool isRtcp = false;
        if (tcp) {
            uint8_t channel;
            std::string data;
            if (!rtsp.TakeInterleaved(channel, data)) {
                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(rtsp.socket, &readable);
                timeval wait = {0, 100 * 1000};
                if (select(0, &readable, nullptr, nullptr, &wait) > 0 && !rtsp.ReceiveMore()) {
                    fprintf(stderr, "the server closed the connection\n");
                    break;
                }
                continue;
            }
            got = int(std::min(data.size(), sizeof(packet)));
            memcpy(packet, data.data(), size_t(got));
            isRtcp = channel == 1;
        } else {
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(rtp, &readable);
            FD_SET(rtcp, &readable);
            timeval wait = {0, 100 * 1000};
            if (select(0, &readable, nullptr, nullptr, &wait) <= 0) continue;
            isRtcp = FD_ISSET(rtcp, &readable);
            got = recv(isRtcp ? rtcp : rtp, reinterpret_cast<char*>(packet), sizeof(packet), 0);
        }

        if (isRtcp) {
            // RTCP arrives as a compound packet; pick out the sender report (type 200).
            for (int at = 0; at + 4 <= got;) {
                int length = int(Get16(packet + at + 2) + 1) * 4;
                if (packet[at + 1] == 200 && at + 20 <= got) {
                    reportNtpMs = Get32(packet + at + 8) * 1000.0 +
                                  Get32(packet + at + 12) * 1000.0 / 4294967296.0;
                    reportRtp = Get32(packet + at + 16);
                    haveReport = true;
                }
                at += length;
            }
            continue;
        }
        if (got < 13 || packet[0] >> 6 != 2 || (packet[1] & 0x7F) != payloadType) continue;
        // Simulated loss looks exactly like real loss from here on: a gap in the sequence.
        if (dropPercent > 0 && rand() % 10000 < dropPercent * 100) continue;
        sourceSsrc = Get32(packet + 8);

        uint16_t seq = uint16_t(Get16(packet + 2));
        uint32_t timestamp = Get32(packet + 4);
        bool marker = packet[1] & 0x80;
        ++packetsTotal, ++interval.packets;
        bool lostBefore = false;
        if (haveSeq && seq != nextSeq) {
            uint16_t gap = uint16_t(seq - nextSeq);
            lostBefore = gap < 0x8000;  // else: reordered
            if (lostBefore) lostTotal += gap, interval.lost += gap;
        }
        haveSeq = true;
        nextSeq = uint16_t(seq + 1);

        if (!assembling || timestamp != frameTimestamp) {
            if (assembling) {  // the previous frame never got its last packet
                ++damagedTotal, ++interval.damaged;
            }
            assembling = true;
            intact = true;
            frameTimestamp = timestamp;
            frame.clear();
            depacketizer.Reset();
        }
        // Loss just before this packet, or a fragment without its start, damages the frame.
        // A player shows it anyway, smeared, until the next key frame.
        if (lostBefore) intact = false;
        if (!depacketizer.Add(packet + 12, size_t(got) - 12, frame)) intact = false;
        if (!marker) continue;
        assembling = false;

        bytesTotal += frame.size();
        interval.bytes += frame.size();
        if (saveFile) fwrite(frame.data(), 1, frame.size(), saveFile);
        gazeora::SplitAnnexB(frame, nalUnits);
        bool keyFrame = std::any_of(nalUnits.begin(), nalUnits.end(), [codec](const std::string& nal) {
            return gazeora::RtpPayloadFormat::For(codec).IsKeyFrame(nal);
        });
        keyFramesTotal += keyFrame, interval.keyFrames += keyFrame;
        if (!intact) {
            ++damagedTotal, ++interval.damaged;
        }
        ++framesTotal, ++interval.frames;

        double arrivedNtp = NowNtpMs(), arrived = NowMs();
        if (haveReport) {
            double capturedNtp = reportNtpMs + int32_t(timestamp - reportRtp) / 90.0;
            double latency = arrivedNtp - capturedNtp;
            latencies.push_back(latency);
            interval.latencySum += latency;
            interval.latencyMax = std::max(interval.latencyMax, latency);
            ++interval.latencyCount;
        }
        if (lastFrameMs) {
            gaps.push_back(arrived - lastFrameMs);
            interval.gapMax = std::max(interval.gapMax, arrived - lastFrameMs);
        }
        lastFrameMs = arrived;
    }
    if (saveFile) fclose(saveFile);
    double elapsed = (NowMs() - start) / 1000.0;
    rtsp.Send("TEARDOWN", base, "Session: " + session + "\r\n");  // no need to wait for it

    printf("\nframes     %d received (%.1f fps), %d damaged by loss, %d key frames\n", framesTotal,
           framesTotal / elapsed, damagedTotal, keyFramesTotal);
    printf("data       %.2f Mbit/s, %.0f KB per frame\n", bytesTotal * 8.0 / elapsed / 1e6,
           framesTotal ? bytesTotal / 1024.0 / framesTotal : 0.0);
    printf("packets    %u received, %u lost (%.2f%%)\n", packetsTotal, lostTotal,
           packetsTotal + lostTotal ? 100.0 * lostTotal / (packetsTotal + lostTotal) : 0.0);
    if (!latencies.empty()) {
        printf("latency    capture to arrival: min %.1f  median %.1f  p95 %.1f  max %.1f ms\n",
               Percentile(latencies, 0), Percentile(latencies, 0.5), Percentile(latencies, 0.95),
               Percentile(latencies, 1));
    } else {
        printf("latency    unknown: no RTCP sender report arrived\n");
    }
    if (!gaps.empty()) {
        printf("gaps       between frames: median %.1f  p95 %.1f  max %.1f ms\n",
               Percentile(gaps, 0.5), Percentile(gaps, 0.95), Percentile(gaps, 1));
    }
    if (savePath && framesTotal) printf("saved      the stream to %s\n", savePath);
    return framesTotal ? 0 : 1;
}
