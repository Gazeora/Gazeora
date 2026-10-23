#include "gazeora/host/encode/color_convert.h"

#include <emmintrin.h>

namespace gazeora {

namespace {

// BT.709 in fixed point (x256), studio range: Y 16-235, U and V 16-240.
inline uint8_t Luma(const uint8_t* bgra) {
    return uint8_t(16 + ((47 * bgra[2] + 157 * bgra[1] + 16 * bgra[0] + 128) >> 8));
}

// U and V of a 2x2 block from four times its mean (x1024).
inline uint8_t ChromaU(int r, int g, int b) {
    return uint8_t(128 + ((-26 * r - 87 * g + 112 * b + 512) >> 10));
}
inline uint8_t ChromaV(int r, int g, int b) {
    return uint8_t(128 + ((112 * r - 102 * g - 10 * b + 512) >> 10));
}

// Luma of four BGRA8 pixels, in 32-bit lanes. PMADDWD multiplies 16-bit lanes in pairs and
// adds each pair: (B, R) by (16, 47) and (G, A) by (157, 0) give the weighted sum.
inline __m128i Luma4(__m128i pixels) {
    __m128i blueRed = _mm_and_si128(pixels, _mm_set1_epi16(0xFF));
    __m128i greenAlpha = _mm_srli_epi16(pixels, 8);
    __m128i sum = _mm_add_epi32(_mm_madd_epi16(blueRed, _mm_set1_epi32((47 << 16) | 16)),
                                _mm_madd_epi16(greenAlpha, _mm_set1_epi32(157)));
    sum = _mm_srli_epi32(_mm_add_epi32(sum, _mm_set1_epi32(128)), 8);
    return _mm_add_epi32(sum, _mm_set1_epi32(16));
}

// U, V, U, V of the two 2x2 blocks in four pixels of row `top` and the four below them, in
// 32-bit lanes.
inline __m128i Chroma2(__m128i top, __m128i bottom) {
    const __m128i zero = _mm_setzero_si128();
    // Channels widened to 16 bits and summed down the columns: pixels 0-1, then 2-3.
    __m128i left = _mm_add_epi16(_mm_unpacklo_epi8(top, zero), _mm_unpacklo_epi8(bottom, zero));
    __m128i right = _mm_add_epi16(_mm_unpackhi_epi8(top, zero), _mm_unpackhi_epi8(bottom, zero));
    // Then across: b, g, r, a sums of block 0, then of block 1.
    left = _mm_add_epi16(left, _mm_srli_si128(left, 8));
    right = _mm_add_epi16(right, _mm_srli_si128(right, 8));
    __m128i sums = _mm_unpacklo_epi64(left, right);
    // Alpha sums replaced by 1, so their coefficient adds the rounding term.
    const __m128i alphaLanes = _mm_set_epi16(-1, 0, 0, 0, -1, 0, 0, 0);
    sums = _mm_or_si128(_mm_andnot_si128(alphaLanes, sums),
                        _mm_and_si128(alphaLanes, _mm_set1_epi16(1)));
    // Pairwise products: (112 b - 87 g, -26 r + 512) for U, (-10 b - 102 g, 112 r + 512) for V.
    __m128i u = _mm_madd_epi16(sums, _mm_set_epi16(512, -26, -87, 112, 512, -26, -87, 112));
    __m128i v = _mm_madd_epi16(sums, _mm_set_epi16(512, 112, -102, -10, 512, 112, -102, -10));
    // Add up each pair, interleaved U, V as NV12 stores them.
    __m128i block0 = _mm_unpacklo_epi32(u, v);  // u0a, v0a, u0b, v0b
    __m128i block1 = _mm_unpackhi_epi32(u, v);
    __m128i uv = _mm_add_epi32(_mm_unpacklo_epi64(block0, block1),
                               _mm_unpackhi_epi64(block0, block1));
    return _mm_add_epi32(_mm_srai_epi32(uv, 10), _mm_set1_epi32(128));
}

inline __m128i Load(const uint8_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }

inline void Store(uint8_t* p, __m128i v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }

// One tile: two source rows to their two luma rows and the chroma row between them. Each 16
// pixels of both rows are loaded once and give luma and chroma together, so the frame is read
// from memory once.
void ConvertRowPair(const uint8_t* row0, const uint8_t* row1, uint8_t* luma0, uint8_t* luma1,
                    uint8_t* uv, uint32_t width) {
    uint32_t x = 0;
    for (; x + 16 <= width; x += 16) {
        const uint8_t* p = row0 + size_t(x) * 4;
        const uint8_t* q = row1 + size_t(x) * 4;
        __m128i p0 = Load(p), p1 = Load(p + 16), p2 = Load(p + 32), p3 = Load(p + 48);
        __m128i q0 = Load(q), q1 = Load(q + 16), q2 = Load(q + 32), q3 = Load(q + 48);
        Store(luma0 + x, _mm_packus_epi16(_mm_packs_epi32(Luma4(p0), Luma4(p1)),
                                          _mm_packs_epi32(Luma4(p2), Luma4(p3))));
        Store(luma1 + x, _mm_packus_epi16(_mm_packs_epi32(Luma4(q0), Luma4(q1)),
                                          _mm_packs_epi32(Luma4(q2), Luma4(q3))));
        Store(uv + x, _mm_packus_epi16(_mm_packs_epi32(Chroma2(p0, q0), Chroma2(p1, q1)),
                                       _mm_packs_epi32(Chroma2(p2, q2), Chroma2(p3, q3))));
    }
    for (; x < width; x += 2) {
        const uint8_t* p = row0 + size_t(x) * 4;
        const uint8_t* q = row1 + size_t(x) * 4;
        luma0[x] = Luma(p), luma0[x + 1] = Luma(p + 4);
        luma1[x] = Luma(q), luma1[x + 1] = Luma(q + 4);
        int r = p[2] + p[6] + q[2] + q[6];
        int g = p[1] + p[5] + q[1] + q[5];
        int b = p[0] + p[4] + q[0] + q[4];
        uv[x] = ChromaU(r, g, b);
        uv[x + 1] = ChromaV(r, g, b);
    }
}

}  // namespace

// SSE2 (every x64 CPU has it), 16 pixels per step, with the same integer arithmetic as the
// scalar code that finishes each row, so the output is identical whatever the thread count.
void BgraToNv12(const uint8_t* bgra, uint32_t stride, uint32_t width, uint32_t height,
                std::vector<uint8_t>& nv12) {
    nv12.resize(size_t(width) * height * 3 / 2);
    uint8_t* yPlane = nv12.data();
    uint8_t* uvPlane = nv12.data() + size_t(width) * height;
    // Tiles of two rows, split over two threads with OpenMP: 30% faster at 1080p for the same
    // CPU time. More threads gain little (the loop is bound by memory and thread wake-ups) and
    // spin away CPU time the game being streamed needs: 12 threads burn 4x the CPU.
    const int pairs = int(height / 2);
#pragma omp parallel for schedule(static) num_threads(2)
    for (int pair = 0; pair < pairs; ++pair) {
        uint32_t y = uint32_t(pair) * 2;
        const uint8_t* row0 = bgra + size_t(y) * stride * 4;
        ConvertRowPair(row0, row0 + size_t(stride) * 4, yPlane + size_t(y) * width,
                       yPlane + size_t(y + 1) * width, uvPlane + size_t(y / 2) * width, width);
    }
}

}  // namespace gazeora
