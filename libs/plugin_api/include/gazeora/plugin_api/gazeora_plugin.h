/* Copyright (C) 2026 Nithin Aashik
 * SPDX-License-Identifier: MIT
 *
 * The Gazeora plugin interface, version 2. A plugin is a DLL in the "plugins" folder next to
 * Gazeora.exe that exports GazeoraPluginInfo and GazeoraPluginRegister. Plain C, so a plugin
 * can be built with any compiler. This header is under the MIT license (libs/plugin_api/LICENSE);
 * see NOTICE for the permission that lets plugins keep their own license.
 *
 * Rules for every function a plugin hands to Gazeora:
 *  - Strings are UTF-8 and NUL-terminated. Text a plugin returns goes into the buffer Gazeora
 *    passes, truncated to its capacity (which counts the NUL).
 *  - Nothing may throw or longjmp out of a plugin function.
 *  - Pointers Gazeora passes are only valid during the call, unless documented otherwise.
 *  - Strings and function pointers in the structs a plugin registers stay valid while the DLL
 *    is loaded. Gazeora copies the structs themselves.
 *
 * Versions: a struct that version 2 extends is a new struct whose first member is the version 1
 * one, and struct_size says which a caller has. Gazeora loads plugins built for version 1 or 2,
 * and reads a plugin's version 2 fields only when its struct_size covers them.
 */

#ifndef GAZEORA_PLUGIN_H
#define GAZEORA_PLUGIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAZEORA_PLUGIN_API_VERSION 2u
/* The oldest version Gazeora still loads. */
#define GAZEORA_PLUGIN_API_OLDEST_VERSION 1u

#define GAZEORA_CALL __cdecl
#ifdef __cplusplus
#define GAZEORA_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define GAZEORA_PLUGIN_EXPORT __declspec(dllexport)
#endif

/* For functions defined in this header, in C and C++ alike. */
#if defined(_MSC_VER) && !defined(__cplusplus)
#define GAZEORA_INLINE static __inline
#else
#define GAZEORA_INLINE static inline
#endif

#define GAZEORA_FALSE 0
#define GAZEORA_TRUE 1

#define GAZEORA_CODEC_H264 0u
#define GAZEORA_CODEC_H265 1u
/* Version 2: a codec version 1 has no number for; GazeoraEncoderSettingsV2 names it. */
#define GAZEORA_CODEC_OTHER 0xFFFFFFFFu

/* What the plugin is, returned by GazeoraPluginInfo. */
typedef struct GazeoraPluginInfoV1 {
    uint32_t struct_size; /* sizeof(GazeoraPluginInfoV1) */
    uint32_t api_version; /* GAZEORA_PLUGIN_API_VERSION the plugin was built with */
    const char* name;     /* for the UI, e.g. "Example store" */
    const char* version;  /* for the UI, e.g. "1.0.0" */
} GazeoraPluginInfoV1;

/* A store's client games can be started through. Called from any thread, also at once. */
typedef struct GazeoraGameStoreV1 {
    uint32_t struct_size; /* sizeof(GazeoraGameStoreV1) */
    void* self;           /* passed back to every function */
    const char* id;       /* stable, lowercase a-z, 0-9 and '-': stored in library.json */
    const char* name;     /* for the UI, e.g. "Example store" */
    /* Writes the store's id for the game the exe belongs to; "" if it isn't one of its games. */
    void(GAZEORA_CALL* app_id_for)(void* self, const char* exe_path, char* app_id, size_t capacity);
    /* Asks the store's client to start the game. GAZEORA_FALSE with `error` set on failure. */
    int32_t(GAZEORA_CALL* launch)(void* self, const char* app_id, char* error, size_t capacity);
} GazeoraGameStoreV1;

/* How to open an encoder. Frames are NV12, width and height even. */
typedef struct GazeoraEncoderSettingsV1 {
    uint32_t struct_size;
    uint32_t codec; /* GAZEORA_CODEC_* */
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t bitrate;      /* bits/s */
    uint32_t key_interval; /* frames between key frames */
    int32_t prefer_hardware;
    /* ID3D11Device*: frames come as textures on it. NULL: frames come from memory. AddRef to
     * keep it past the call. */
    void* d3d11_device;
} GazeoraEncoderSettingsV1;

/* One encoded access unit: NAL units with start codes, no B-frames. */
typedef struct GazeoraEncodedFrameV1 {
    uint32_t struct_size;
    const uint8_t* annexb;
    size_t size;
    int64_t time; /* the input's time, in 100 ns units */
    int32_t key_frame;
} GazeoraEncodedFrameV1;

/* Where an encoder hands each finished frame; Gazeora copies it during the call. */
typedef void(GAZEORA_CALL* GazeoraFrameSinkV1)(void* sink_context,
                                               const GazeoraEncodedFrameV1* frame);

/* One encoder, used from a single thread that has COM (multithreaded) and MFStartup. */
typedef struct GazeoraEncoderV1 {
    uint32_t struct_size;
    void* self;
    int32_t(GAZEORA_CALL* open)(void* self, const GazeoraEncoderSettingsV1* settings, char* error,
                                size_t capacity);
    void(GAZEORA_CALL* close)(void* self);
    int32_t(GAZEORA_CALL* is_hardware)(void* self);
    void(GAZEORA_CALL* set_bitrate)(void* self, uint32_t bitrate);
    void(GAZEORA_CALL* request_key_frame)(void* self);
    /* Encodes one tightly packed NV12 frame of the opened size. */
    int32_t(GAZEORA_CALL* encode)(void* self, const uint8_t* nv12, int64_t time,
                                  GazeoraFrameSinkV1 sink, void* sink_context, char* error,
                                  size_t capacity);
    /* The same for an ID3D11Texture2D* on the opened device. NULL if it takes no textures. */
    int32_t(GAZEORA_CALL* encode_texture)(void* self, void* nv12_texture, int64_t time,
                                          GazeoraFrameSinkV1 sink, void* sink_context, char* error,
                                          size_t capacity);
    /* Frees the encoder; nothing is called on it after. */
    void(GAZEORA_CALL* destroy)(void* self);
} GazeoraEncoderV1;

/* A way of encoding. Gazeora tries plugin encoders before its own. Called from any thread. */
typedef struct GazeoraEncoderProviderV1 {
    uint32_t struct_size;
    void* self;
    const char* name; /* for logs, e.g. "Example encoder" */
    int32_t(GAZEORA_CALL* supports)(void* self, uint32_t codec);
    /* Fills `encoder` with a new, closed encoder. */
    int32_t(GAZEORA_CALL* create)(void* self, GazeoraEncoderV1* encoder);
} GazeoraEncoderProviderV1;

/* Version 2: an encoder provider that can say whether it encodes any codec, by its id. */
typedef struct GazeoraEncoderProviderV2 {
    GazeoraEncoderProviderV1 v1; /* v1.struct_size = sizeof(GazeoraEncoderProviderV2) */
    /* Whether it encodes the codec with this id ("h264", "h265", or a plugin codec's). Used
     * instead of v1.supports. */
    int32_t(GAZEORA_CALL* supports_id)(void* self, const char* codec_id);
} GazeoraEncoderProviderV2;

/* Version 2: what an encoder of a GazeoraEncoderProviderV2 gets in open. v1.codec is
 * GAZEORA_CODEC_OTHER for a codec version 1 has no number for. */
typedef struct GazeoraEncoderSettingsV2 {
    GazeoraEncoderSettingsV1 v1; /* v1.struct_size = sizeof(GazeoraEncoderSettingsV2) */
    const char* codec_id;
} GazeoraEncoderSettingsV2;

/* ---- Version 2: codecs ---- */

/* Bytes a plugin hands over, or Gazeora hands a plugin. */
typedef struct GazeoraBytesV2 {
    const uint8_t* data;
    size_t size;
} GazeoraBytesV2;

#define GAZEORA_PARAMETER_SET_NONE 0u
#define GAZEORA_PARAMETER_SET_VPS 1u
#define GAZEORA_PARAMETER_SET_SPS 2u
#define GAZEORA_PARAMETER_SET_PPS 3u
#define GAZEORA_PARAMETER_SET_COUNT 4u

/* Where a codec's depacketizer appends bytes. */
typedef void(GAZEORA_CALL* GazeoraBytesSinkV2)(void* sink_context, const uint8_t* data,
                                               size_t size);

/* A codec whose bitstream is NAL units in Annex B (H.264, H.265, H.266 and the like): how the
 * stream carries it. Encoding it is an encoder provider's job. Gazeora builds the RTP packets: a
 * NAL unit that fits goes in a packet of its own, a larger one is cut into fragmentation units
 * (the payload header from fragment_payload_header, then a byte with the start and end bits and
 * the NAL type). NAL units are passed without their start codes. Called from any thread. */
typedef struct GazeoraCodecV2 {
    uint32_t struct_size; /* sizeof(GazeoraCodecV2) */
    void* self;
    const char* id;     /* stable, lowercase a-z, 0-9 and '-': stream.ini and discovery carry it */
    const char* name;   /* for the UI, e.g. "H.266" */
    const char* choice; /* its line in the settings' codec list */
    uint32_t bitrate_percent; /* of the bitrate H.264 needs for about the same picture */

    const char* rtp_encoding_name; /* SDP, as in "a=rtpmap:96 H266/90000" */
    uint32_t nal_header_size;      /* 1 or 2 bytes */
    uint32_t(GAZEORA_CALL* nal_type)(void* self, const uint8_t* nal, size_t size);
    /* A NAL unit a player can start decoding from. */
    int32_t(GAZEORA_CALL* is_key_frame)(void* self, const uint8_t* nal, size_t size);
    /* A NAL unit with picture data, which the view info SEI has to come before. */
    int32_t(GAZEORA_CALL* is_picture)(void* self, const uint8_t* nal, size_t size);
    int32_t(GAZEORA_CALL* is_access_unit_delimiter)(void* self, const uint8_t* nal, size_t size);
    /* GAZEORA_PARAMETER_SET_*. */
    uint32_t(GAZEORA_CALL* parameter_set_of)(void* self, const uint8_t* nal, size_t size);
    /* The parameter sets a decoder needs before a key frame, in order; the rest NONE. */
    uint32_t required_parameter_sets[GAZEORA_PARAMETER_SET_COUNT - 1];
    /* The SDP fmtp parameters for the parameter sets seen so far (indexed by
     * GAZEORA_PARAMETER_SET_*, size 0 for one not seen); "" when there's nothing to say. */
    void(GAZEORA_CALL* fmtp)(void* self, const GazeoraBytesV2* sets, char* fmtp, size_t capacity);
    /* The fragmentation unit payload header for `nal`, written to `header`; returns its size. */
    size_t(GAZEORA_CALL* fragment_payload_header)(void* self, const uint8_t* nal, size_t size,
                                                  uint8_t* header, size_t capacity);
    /* Receiver side, for tools; may be NULL. Appends the NAL units in one RTP payload, with
     * start codes. `in_fragment` carries a half-received fragment between calls. */
    int32_t(GAZEORA_CALL* depacketize)(void* self, const uint8_t* payload, size_t size,
                                       int32_t* in_fragment, GazeoraBytesSinkV2 sink,
                                       void* sink_context);

    uint8_t ts_stream_type;               /* MPEG-TS stream_type */
    GazeoraBytesV2 access_unit_delimiter; /* with its start code */
    GazeoraBytesV2 sei_header;            /* an SEI NAL unit's header; size 0: no view info */
} GazeoraCodecV2;

/* ---- Version 2: protocols ---- */

/* One encoded frame for an output: valid only during send. */
typedef struct GazeoraFrameV2 {
    uint32_t struct_size;
    const uint8_t* annexb; /* NAL units with start codes, parameter sets in front of key frames */
    size_t size;
    uint32_t timestamp; /* 90 kHz */
    int32_t key_frame;
    const char* codec_id;
    uint32_t width;
    uint32_t height;
    uint32_t bitrate; /* the stream's, bits/s */
    uint32_t fps;
    /* How the codec is carried, built-in codecs too: valid while the output exists. */
    const GazeoraCodecV2* codec;
} GazeoraFrameV2;

/* Where an output reports the loss its players see, so the stream backs off: valid only during
 * send. `player` is any number that tells the output's players apart. */
typedef struct GazeoraLossReporterV2 {
    void* context;
    void(GAZEORA_CALL* report)(void* context, uint64_t player, float fraction_lost);
} GazeoraLossReporterV2;

/* Messages a player sends on an output's own link (a cable, say), in the Gazeora app's format
 * (the payload of the GZF1 message of that type, docs/stream-format.md). */
#define GAZEORA_INPUT_POINTER 5u
#define GAZEORA_INPUT_HEAD_POSE 7u
#define GAZEORA_INPUT_CAMERA_STATUS 8u

/* What Gazeora offers an output, valid while the output exists. */
typedef struct GazeoraOutputHostV2 {
    uint32_t struct_size;
    void* context;
    /* A line in the stream log; from any thread. */
    void(GAZEORA_CALL* log)(void* context, const char* message);
    /* Hands Gazeora a message a player sent on the output's own link: `type` is
     * GAZEORA_INPUT_*, others are ignored. Input sent over the network Gazeora hears itself, from
     * the players `watching` admits. From any thread. */
    void(GAZEORA_CALL* input)(void* context, uint32_t type, const uint8_t* payload, size_t size);
} GazeoraOutputHostV2;

/* Where an output lists the USB devices that need the WinUSB driver Gazeora installs. */
typedef void(GAZEORA_CALL* GazeoraUsbDeviceSinkV2)(void* sink_context, const char* hardware_id,
                                                   const char* name);

/* One running protocol: takes players and sends them frames. start, stop and send are called
 * one at a time; the rest may be called from any thread at any time. The output runs its own
 * threads for its players. The functions marked optional may be NULL. */
typedef struct GazeoraOutputV2 {
    uint32_t struct_size;
    void* self;
    /* Takes players on `ipv4` (network byte order), on `port` if free (0: any). */
    int32_t(GAZEORA_CALL* start)(void* self, uint32_t ipv4, uint16_t port, char* error,
                                 size_t capacity);
    void(GAZEORA_CALL* stop)(void* self);
    uint16_t(GAZEORA_CALL* port)(void* self); /* 0 if it has none */
    int32_t(GAZEORA_CALL* viewers)(void* self);
    /* GAZEORA_TRUE once after a player joined or lost a frame: send a key frame next. */
    int32_t(GAZEORA_CALL* take_key_frame_request)(void* self);
    void(GAZEORA_CALL* send)(void* self, const GazeoraFrameV2* frame,
                             const GazeoraLossReporterV2* loss);
    /* What a player opens to watch from `host`; "" for none. */
    void(GAZEORA_CALL* url)(void* self, const char* host, char* url, size_t capacity);
    /* A line on how it stands, or "". Optional. */
    void(GAZEORA_CALL* status)(void* self, char* status, size_t capacity);
    /* Frees it; nothing is called on it after. */
    void(GAZEORA_CALL* destroy)(void* self);
    /* The viewers that are the Gazeora app. Optional: NULL counts every viewer. */
    int32_t(GAZEORA_CALL* app_viewers)(void* self);
    /* Whether a player at `ipv4` (network byte order) is watching: Gazeora takes touches and
     * head poses sent over the network only from one. Optional: NULL takes none. */
    int32_t(GAZEORA_CALL* watching)(void* self, uint32_t ipv4);
    /* Whether a phone is waiting for the USB driver Gazeora installs. Optional. */
    int32_t(GAZEORA_CALL* needs_usb_driver)(void* self);
    /* Lists the USB devices that need that driver. Optional. */
    void(GAZEORA_CALL* usb_devices_needing_driver)(void* self, GazeoraUsbDeviceSinkV2 sink,
                                                   void* sink_context);
} GazeoraOutputV2;

/* A protocol: how frames leave the PC. */
typedef struct GazeoraProtocolV2 {
    uint32_t struct_size; /* sizeof(GazeoraProtocolV2) */
    void* self;
    const char* id;   /* stable, lowercase a-z, 0-9 and '-': stream.ini and discovery carry it */
    const char* name; /* for the UI, e.g. "SRT" */
    const char* help; /* what it's good for, in the settings' help */
    const char* discovery_path; /* the path the Android app opens, e.g. "/" */
    int32_t opened_by_players;  /* whether players such as VLC open its URL */
    /* Fills `output` with a new, stopped output. */
    int32_t(GAZEORA_CALL* create_output)(void* self, const GazeoraOutputHostV2* host,
                                         GazeoraOutputV2* output);
} GazeoraProtocolV2;

/* What Gazeora offers a plugin while it registers. Valid only during GazeoraPluginRegister. */
typedef struct GazeoraPluginHostV1 {
    uint32_t struct_size;
    uint32_t api_version; /* the newest version this Gazeora supports */
    void* context;        /* passed back to every function */
    int32_t(GAZEORA_CALL* register_game_store)(void* context, const GazeoraGameStoreV1* store);
    int32_t(GAZEORA_CALL* register_encoder_provider)(void* context,
                                                     const GazeoraEncoderProviderV1* provider);
} GazeoraPluginHostV1;

/* Version 2: what Gazeora offers a plugin built for version 2. */
typedef struct GazeoraPluginHostV2 {
    GazeoraPluginHostV1 v1; /* v1.struct_size = sizeof(GazeoraPluginHostV2) */
    int32_t(GAZEORA_CALL* register_codec)(void* context, const GazeoraCodecV2* codec);
    int32_t(GAZEORA_CALL* register_protocol)(void* context, const GazeoraProtocolV2* protocol);
} GazeoraPluginHostV2;

/* The version 2 host, or NULL from a Gazeora that only has version 1. */
GAZEORA_INLINE const GazeoraPluginHostV2* GazeoraPluginHostV2Of(const GazeoraPluginHostV1* host) {
    return host && host->api_version >= 2u && host->struct_size >= sizeof(GazeoraPluginHostV2)
               ? (const GazeoraPluginHostV2*)host
               : NULL;
}

/* The two functions a plugin exports, by these names. */
#define GAZEORA_PLUGIN_INFO_SYMBOL "GazeoraPluginInfo"
#define GAZEORA_PLUGIN_REGISTER_SYMBOL "GazeoraPluginRegister"
typedef const GazeoraPluginInfoV1*(GAZEORA_CALL* GazeoraPluginInfoFn)(void);
/* Registers what the plugin offers. GAZEORA_FALSE drops everything it registered. */
typedef int32_t(GAZEORA_CALL* GazeoraPluginRegisterFn)(const GazeoraPluginHostV1* host);

#ifdef __cplusplus
}
#endif

#endif /* GAZEORA_PLUGIN_H */
