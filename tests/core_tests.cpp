// Testy rdzenia kroku 0: tozsamosc co do bitu, niezaleznosc od kafelkow i watkow, przezroczystosc, zakres 16 bit.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "core/pipeline.h"

using namespace zk;

static int g_fail = 0;
#define CHECK(c, msg)                                   \
    do {                                                \
        if (!(c)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
        else std::printf("ok:   %s\n", msg);             \
    } while (0)

struct Img {
    int w, h, planes, bps;
    std::vector<uint8_t> d;
    int rowBytes() const { return w * planes * bps; }
};

static Img MakeImg(int w, int h, int planes, int bps, uint32_t seed) {
    Img im{w, h, planes, bps, {}};
    im.d.resize((size_t)im.rowBytes() * h);
    std::mt19937 rng(seed);
    if (bps == 1) {
        for (auto& b : im.d) b = (uint8_t)(rng() & 0xFF);
    } else {
        uint16_t* p = (uint16_t*)im.d.data();
        for (size_t i = 0; i < im.d.size() / 2; ++i) p[i] = (uint16_t)(rng() % 32769);
    }
    // Kilka pikseli w pelni przezroczystych.
    if (planes == 4) {
        for (int i = 0; i < w * h; i += 7) {
            uint8_t* px = &im.d[(size_t)i * planes * bps];
            if (bps == 1) px[3] = 0; else ((uint16_t*)px)[3] = 0;
        }
    }
    return im;
}

static std::vector<uint8_t> Whole(const PipelineOptions& o, const Img& im, int threads) {
    std::vector<uint8_t> out(im.d.size());
    ProcessRect(o, im.d.data(), im.rowBytes(), out.data(), im.rowBytes(), im.w, im.h, im.planes, im.bps, 100, 200,
                nullptr, 0, threads);
    return out;
}

static std::vector<uint8_t> Tiled(const PipelineOptions& o, const Img& im, int tw, int th, int threads) {
    std::vector<uint8_t> out(im.d.size());
    const size_t px = (size_t)im.planes * im.bps;
    for (int ty = 0; ty < im.h; ty += th) {
        for (int tx = 0; tx < im.w; tx += tw) {
            int w = std::min(tw, im.w - tx), h = std::min(th, im.h - ty);
            ProcessRect(o, im.d.data() + (size_t)ty * im.rowBytes() + tx * px, im.rowBytes(),
                        out.data() + (size_t)ty * im.rowBytes() + tx * px, im.rowBytes(), w, h, im.planes, im.bps,
                        100 + tx, 200 + ty, nullptr, 0, threads);
        }
    }
    return out;
}

#include "cube_tests.inc"
#include "curve_tests.inc"
#include "library_tests.inc"
#include "bw_tests.inc"
#include "duotone_tests.inc"
#include "new_tests.inc"

int main() {
    for (int bps = 1; bps <= 2; ++bps) {
        for (int planes = 3; planes <= 4; ++planes) {
            char tag[64];
            std::snprintf(tag, sizeof tag, "[%d bit, %d plaszczyzn]", bps * 8, planes);
            Img im = MakeImg(301, 177, planes, bps, 1234 + bps * 10 + planes);

            PipelineOptions id;
            id.strength = 0.0f;
            id.dither = true;
            std::string m = std::string("sila 0 + dithering = wynik identyczny co do bitu ") + tag;
            CHECK(Whole(id, im, 4) == im.d, m.c_str());

            for (int e = 0; e < (int)TestEffect::Count; ++e) {
                PipelineOptions o;
                o.effect = (TestEffect)e;
                o.strength = 0.7f;
                o.dither = true;
                auto ref = Whole(o, im, 1);
                m = std::string("niezaleznosc od watkow (1 vs 8), efekt ") + std::to_string(e) + " " + tag;
                CHECK(Whole(o, im, 8) == ref, m.c_str());
                m = std::string("niezaleznosc od kafelkow (37x53, 256x16), efekt ") + std::to_string(e) + " " + tag;
                CHECK(Tiled(o, im, 37, 53, 3) == ref && Tiled(o, im, 256, 16, 2) == ref, m.c_str());

                bool alphaOk = true, transOk = true, rangeOk = true;
                const size_t px = (size_t)planes * bps;
                for (size_t i = 0; i < (size_t)im.w * im.h; ++i) {
                    const uint8_t* a = &im.d[i * px];
                    const uint8_t* b = &ref[i * px];
                    if (planes == 4) {
                        if (std::memcmp(a + 3 * bps, b + 3 * bps, bps) != 0) alphaOk = false;
                        bool transparent = bps == 1 ? a[3] == 0 : ((const uint16_t*)a)[3] == 0;
                        if (transparent && std::memcmp(a, b, px) != 0) transOk = false;
                    }
                    if (bps == 2)
                        for (int c = 0; c < planes; ++c)
                            if (((const uint16_t*)b)[c] > 32768) rangeOk = false;
                }
                m = std::string("przezroczystosc nietknieta, piksele alfa=0 pominiete, zakres ") + std::to_string(e) +
                    " " + tag;
                CHECK(alphaOk && transOk && rangeOk, m.c_str());
            }

            // Maska 0 = piksel bez zmian.
            PipelineOptions o;
            o.effect = TestEffect::Invert;
            std::vector<uint8_t> mask((size_t)im.w * im.h, 0), out(im.d.size());
            ProcessRect(o, im.d.data(), im.rowBytes(), out.data(), im.rowBytes(), im.w, im.h, planes, bps, 0, 0,
                        mask.data(), im.w, 4);
            m = std::string("maska 0 = obraz bez zmian ") + tag;
            CHECK(out == im.d, m.c_str());
        }
    }
    // Szum ditheringu w zakresie (-0.5, 0.5).
    float mn = 1, mx = -1;
    for (int y = -50; y < 50; ++y)
        for (int x = -50; x < 500; ++x)
            for (int c = 0; c < 3; ++c) {
                float v = DitherNoise(x, y, c);
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
    CHECK(mn > -0.5f && mx < 0.5f && mn < -0.45f && mx > 0.45f, "dithering w zakresie (-0,5; 0,5) LSB");

    RunCubeTests();
    RunCurveTests();
    RunLibraryTests();
    RunBwTests();
    RunDuotoneTests();
    RunNewTests();

    std::printf(g_fail ? "\n%d BLEDOW\n" : "\nWSZYSTKO OK\n", g_fail);
    return g_fail ? 1 : 0;
}
