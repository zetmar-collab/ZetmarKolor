#include "core/pipeline.h"

#include "core/cube_lut.h"
#include "core/bw_toning.h"
#include "core/curves.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <thread>

namespace zk {

namespace {

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Wyrazna S-krzywa (smoothstep zmieszany z tozsamoscia), monotoniczna na 0..1.
inline float SCurve(float v) {
    float s = v * v * (3.0f - 2.0f * v);
    return v + (s - v) * 0.8f;
}

inline uint32_t Hash(uint32_t a) {
    a ^= a >> 16; a *= 0x7feb352dU;
    a ^= a >> 15; a *= 0x846ca68bU;
    a ^= a >> 16;
    return a;
}

}  // namespace

void ProcessPixel(const PipelineOptions& o, const float src[3], float out[3]) {
    float in[3] = {src[0], src[1], src[2]};
    if (o.curves) ApplyCurves(*o.curves, in);
    float r[3];
    switch (o.effect) {
        case TestEffect::Invert:
            for (int c = 0; c < 3; ++c) r[c] = 1.0f - in[c];
            break;
        case TestEffect::Gray: {
            float y = 0.2126f * in[0] + 0.7152f * in[1] + 0.0722f * in[2];
            r[0] = r[1] = r[2] = y;
            break;
        }
        case TestEffect::SCurve:
            for (int c = 0; c < 3; ++c) r[c] = SCurve(in[c]);
            break;
        default:
            for (int c = 0; c < 3; ++c) r[c] = in[c];
            break;
    }
    for (int c = 0; c < 3; ++c) out[c] = Clamp01(in[c] + (r[c] - in[c]) * o.strength);
    if (o.bw) ApplyBw(*o.bw, out);
    if (o.duotone && o.duotoneMix) ApplyDuotone(*o.duotone, o.duotoneMix, out);
    if (o.lut && o.lutStrength > 0.0f) {
        float l[3];
        SampleCube(*o.lut, out, l, o.lutTrilinear);
        for (int c = 0; c < 3; ++c) out[c] = Clamp01(out[c] + (l[c] - out[c]) * o.lutStrength);
    }
    if (o.toning) ApplyToning(*o.toning, out);
    if (o.overall < 1.0f)
        for (int c = 0; c < 3; ++c) out[c] = src[c] + (out[c] - src[c]) * o.overall;
}

float DitherNoise(int64_t x, int64_t y, int channel) {
    uint32_t h = Hash((uint32_t)x * 0x9E3779B1U ^ Hash((uint32_t)y + 0x632BE5ABU) ^ ((uint32_t)channel * 0x85EBCA77U));
    // 24 bity -> [0,1) -> (-0.49, 0.49)
    float u = (float)(h >> 8) * (1.0f / 16777216.0f);
    return (u - 0.5f) * 0.98f;
}

void ProcessRow(const PipelineOptions& o, const uint8_t* in, uint8_t* out, int width, int planes, int bps,
                int64_t absX, int64_t absY, const uint8_t* mask) {
    const float maxv = bps == 2 ? 32768.0f : 255.0f;
    const size_t stride = (size_t)planes * bps;
    std::memcpy(out, in, stride * (size_t)width);  // przezroczystosc i piksele pominiete bez zmian
    for (int x = 0; x < width; ++x) {
        const uint8_t* ip = in + stride * x;
        uint8_t* op = out + stride * x;
        float m = mask ? mask[x] / 255.0f : 1.0f;
        if (m <= 0.0f) continue;
        float v[3];
        if (bps == 1) {
            if (planes > 3 && ip[3] == 0) continue;  // w pelni przezroczysty: nie przetwarzamy
            for (int c = 0; c < 3; ++c) v[c] = ip[c] / maxv;
        } else {
            const uint16_t* i16 = (const uint16_t*)ip;
            if (planes > 3 && i16[3] == 0) continue;
            for (int c = 0; c < 3; ++c) v[c] = std::min(i16[c] / maxv, 1.0f);
        }
        float r[3];
        ProcessPixel(o, v, r);
        for (int c = 0; c < 3; ++c) {
            float f = (v[c] + (r[c] - v[c]) * m) * maxv;
            if (o.dither) f += DitherNoise(absX + x, absY, c);
            f = std::floor(f + 0.5f);
            f = f < 0.0f ? 0.0f : (f > maxv ? maxv : f);
            if (bps == 1) op[c] = (uint8_t)f;
            else ((uint16_t*)op)[c] = (uint16_t)f;
        }
    }
}

void ParallelRows(int height, int threads, const std::function<void(int)>& fn) {
    if (height <= 0) return;
    if (threads <= 0) threads = (int)std::max(1u, std::thread::hardware_concurrency());
    threads = std::min(std::min(threads, 64), height);
    // Wiersze rozdawane dynamicznie: gdy nie uda sie utworzyc wszystkich watkow, reszte wykona biezacy.
    std::atomic<int> next{0};
    auto work = [&]() {
        for (int y; (y = next.fetch_add(1)) < height;) fn(y);
    };
    std::vector<std::thread> pool;
    if (threads > 1) {
        pool.reserve((size_t)threads - 1);
        try {
            for (int t = 1; t < threads; ++t) pool.emplace_back(work);
        } catch (...) {
            // Brak zasobow na kolejne watki: pracujemy na tylu, ile sie udalo (nie niszczymy niedolaczonych watkow).
        }
    }
    work();
    for (auto& th : pool) th.join();
}

void ProcessRect(const PipelineOptions& o, const uint8_t* in, int inRowBytes, uint8_t* out, int outRowBytes,
                 int width, int height, int planes, int bps, int64_t absX, int64_t absY, const uint8_t* mask,
                 int maskRowBytes, int threads) {
    if (width <= 0 || height <= 0) return;
    ParallelRows(height, threads, [&](int y) {
        ProcessRow(o, in + (ptrdiff_t)y * inRowBytes, out + (ptrdiff_t)y * outRowBytes, width, planes, bps, absX,
                   absY + y, mask ? mask + (ptrdiff_t)y * maskRowBytes : nullptr);
    });
}

}  // namespace zk
