#include "color_match.h"

#include <climits>
#include <cstdint>
#include <vector>

// SSE2 is guaranteed on x64 (which we target). The intrinsics let us
// classify 4 BGRA pixels per iteration when building the match mask,
// which is the hot loop when scanning full-screen captures.
// Note: requires -msse2 (and -mssse3 for pshufb) on the compile line.
#include <emmintrin.h>
#include <tmmintrin.h>   // SSSE3 — pshufb

namespace aimlab {

namespace {

inline int sqr(int v) { return v * v; }

inline int pixelDistSq(const std::uint8_t* px, int tr, int tg, int tb) {
    int dr = static_cast<int>(px[2]) - tr;
    int dg = static_cast<int>(px[1]) - tg;
    int db = static_cast<int>(px[0]) - tb;
    return dr * dr + dg * dg + db * db;
}

// Process 4 BGRA pixels at once. Writes 4 mask bytes (0 or 1) to mask_out.
// Each pixel is compared against (tr,tg,tb); dist_sq <= thr3 counts as a match.
inline void simd_match4(const std::uint8_t* px,
                        std::uint8_t* mask_out,
                        int tr, int tg, int tb, int thr3) {
    // Load 16 raw bytes (4 BGRA pixels).
    __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(px));

    // Use pshufb to extract B (offsets 0,4,8,12), G (1,5,9,13), R (2,6,10,14)
    // into the low 4 bytes of separate vectors. Upper bytes are zeroed.
    const __m128i bMask = _mm_set_epi8(
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        12,  8,  4,  0);
    const __m128i gMask = _mm_set_epi8(
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        13,  9,  5,  1);
    const __m128i rMask = _mm_set_epi8(
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        (char)0x80,(char)0x80,(char)0x80,(char)0x80,
        14, 10,  6,  2);

    __m128i bPack = _mm_shuffle_epi8(raw, bMask);
    __m128i gPack = _mm_shuffle_epi8(raw, gMask);
    __m128i rPack = _mm_shuffle_epi8(raw, rMask);

    // Unpack low 4 bytes from 8-bit to 16-bit (zero-extend).
    const __m128i zero = _mm_setzero_si128();
    __m128i b16 = _mm_unpacklo_epi8(bPack, zero);
    __m128i g16 = _mm_unpacklo_epi8(gPack, zero);
    __m128i r16 = _mm_unpacklo_epi8(rPack, zero);

    // (channel - target), broadcast target.
    __m128i bD = _mm_sub_epi16(b16, _mm_set1_epi16(static_cast<short>(tb)));
    __m128i gD = _mm_sub_epi16(g16, _mm_set1_epi16(static_cast<short>(tg)));
    __m128i rD = _mm_sub_epi16(r16, _mm_set1_epi16(static_cast<short>(tr)));

    // Square via madd_epi16 (sums pairs of products into 32-bit lanes:
    // 4 x (0² + 0², B² + G², ...). To get just B²/G²/R² we feed a vector
    // with alternating B and 0.
    __m128i bBG = _mm_unpacklo_epi16(bD, zero);   // [B0, 0, B1, 0, B2, 0, B3, 0]  (16-bit)
    __m128i bD2 = _mm_madd_epi16(bBG, bBG);       // [B0², B1², B2², B3²] (32-bit)
    __m128i gBG = _mm_unpacklo_epi16(gD, zero);
    __m128i gD2 = _mm_madd_epi16(gBG, gBG);
    __m128i rBG = _mm_unpacklo_epi16(rD, zero);
    __m128i rD2 = _mm_madd_epi16(rBG, rBG);

    __m128i sum = _mm_add_epi32(_mm_add_epi32(bD2, gD2), rD2);
    __m128i thr = _mm_set1_epi32(thr3);
    __m128i match = _mm_cmplt_epi32(sum, thr);    // 0xFFFFFFFF if sum<thr, else 0

    // High bit of each 32-bit lane -> 4-bit mask.
    int bits = _mm_movemask_ps(_mm_castsi128_ps(match));
    mask_out[0] = static_cast<std::uint8_t>((bits >> 0) & 1);
    mask_out[1] = static_cast<std::uint8_t>((bits >> 1) & 1);
    mask_out[2] = static_cast<std::uint8_t>((bits >> 2) & 1);
    mask_out[3] = static_cast<std::uint8_t>((bits >> 3) & 1);
}

}  // namespace

TargetMatch FindTarget(const ScreenFrame& frame,
                       int anchorX, int anchorY,
                       int tr, int tg, int tb,
                       int tolerance,
                       int minBlobPx,
                       int clusterRadius,
                       int minCluster,
                       int preferredX,
                       int preferredY,
                       int stickRadius) {
    TargetMatch best;
    if (!frame.valid() || tolerance < 0) return best;
    if (minBlobPx   < 1) minBlobPx   = 1;
    if (clusterRadius < 1) clusterRadius = 1;
    if (minCluster  < 1) minCluster  = 1;

    const int w = frame.width;
    const int h = frame.height;
    const std::uint8_t* data = frame.pixels.data();
    const int tol2 = sqr(tolerance) * 3;

    // Pass 1: build a binary "matches" mask. SIMD over 4-pixel batches
    // when w is a multiple of 4; scalar tail handles the remainder.
    std::vector<unsigned char> mask(static_cast<std::size_t>(w) * h, 0);
    int matchCount = 0;
    const int w4 = w & ~3;            // largest multiple of 4 <= w
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* row = data + static_cast<std::size_t>(y) * w * 4;
        std::uint8_t* maskRow = mask.data() + static_cast<std::size_t>(y) * w;
        int x = 0;
        for (; x < w4; x += 4) {
            std::uint8_t result[4];
            simd_match4(row + x * 4, result, tr, tg, tb, tol2);
            maskRow[x + 0] = result[0];
            maskRow[x + 1] = result[1];
            maskRow[x + 2] = result[2];
            maskRow[x + 3] = result[3];
            matchCount += result[0] + result[1] + result[2] + result[3];
        }
        for (; x < w; ++x) {
            if (pixelDistSq(row + x * 4, tr, tg, tb) <= tol2) {
                maskRow[x] = 1;
                ++matchCount;
            }
        }
    }
    if (matchCount == 0) return best;

    // Pass 2: integral image over the mask. ii is (w+1) x (h+1) so we can
    // query axis-aligned rectangles in O(1).
    const std::size_t iiW = static_cast<std::size_t>(w) + 1;
    std::vector<int> ii(iiW * (static_cast<std::size_t>(h) + 1), 0);
    for (int y = 0; y < h; ++y) {
        int rowSum = 0;
        const std::size_t base = static_cast<std::size_t>(y) * w;
        const std::size_t row1 = static_cast<std::size_t>(y + 1) * iiW;
        const std::size_t row0 = static_cast<std::size_t>(y)     * iiW;
        for (int x = 0; x < w; ++x) {
            rowSum += mask[base + x];
            ii[row1 + (x + 1)] = ii[row0 + (x + 1)] + rowSum;
        }
    }

    // Helper: count of matches inside the inclusive box [x1,x2]x[y1,y2]
    // (clamped to the image).
    auto rectSum = [&](int x1, int y1, int x2, int y2) -> int {
        if (x1 < 0)     x1 = 0;
        if (y1 < 0)     y1 = 0;
        if (x2 > w - 1) x2 = w - 1;
        if (y2 > h - 1) y2 = h - 1;
        const std::size_t a = static_cast<std::size_t>(y2 + 1) * iiW + (x2 + 1);
        const std::size_t b = static_cast<std::size_t>(y1)     * iiW + (x2 + 1);
        const std::size_t c = static_cast<std::size_t>(y2 + 1) * iiW + (x1);
        const std::size_t d = static_cast<std::size_t>(y1)     * iiW + (x1);
        return ii[a] - ii[b] - ii[c] + ii[d];
    };

    // Pass 3: pre-filter with the 3x3 neighborhood (cheap), then collect
    // all surviving candidates with their density. We pick the winner
    // after the scan so the sticky rule can override the anchor rule.
    struct Cand { int x, y, density, anchorD2; };
    std::vector<Cand> cands;
    cands.reserve(64);
    const int R = clusterRadius;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[static_cast<std::size_t>(y) * w + x]) continue;

            int n3 = rectSum(x - 1, y - 1, x + 1, y + 1);
            if (n3 < minBlobPx) continue;

            int density = rectSum(x - R, y - R, x + R, y + R);
            if (density < minCluster) continue;

            int ddx = x - anchorX;
            int ddy = y - anchorY;
            cands.push_back({x, y, density, ddx * ddx + ddy * ddy});
        }
    }
    if (cands.empty()) return best;

    // Sticky rule: if a hint is provided, try to find a candidate near it.
    // The candidate must pass the same density filter (already done above)
    // and lie within stickRadius of the hint.
    if (preferredX >= 0) {
        int bestHintD2 = INT_MAX;
        int bestIdx    = -1;
        const int stickR2 = stickRadius * stickRadius;
        for (int i = 0; i < (int)cands.size(); ++i) {
            int dx = cands[i].x - preferredX;
            int dy = cands[i].y - preferredY;
            int d2 = dx * dx + dy * dy;
            if (d2 <= stickR2 && d2 < bestHintD2) {
                bestHintD2 = d2;
                bestIdx    = i;
            }
        }
        if (bestIdx >= 0) {
            best.found = true;
            best.x     = cands[bestIdx].x;
            best.y     = cands[bestIdx].y;
            best.size  = cands[bestIdx].density;
            return best;
        }
        // No candidate close to the hint: fall through to the normal
        // "closest to anchor" selection below.
    }

    // Normal rule: highest density wins, ties broken by anchor distance.
    int bestCluster  = 0;
    int bestAnchorD2 = INT_MAX;
    for (const auto& c : cands) {
        if (c.density > bestCluster ||
            (c.density == bestCluster && c.anchorD2 < bestAnchorD2)) {
            bestCluster  = c.density;
            bestAnchorD2 = c.anchorD2;
            best.found   = true;
            best.x       = c.x;
            best.y       = c.y;
            best.size    = c.density;
        }
    }
    return best;
}

}  // namespace aimlab
