// Potok przetwarzania pikseli (float 0..1) i zapis z deterministycznym ditheringiem.
// Bez zaleznosci od Photoshopa (testowane w tests/core_tests.cpp).
#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace zk {

enum class TestEffect : int32_t { None = 0, SCurve = 1, Invert = 2, Gray = 3, Count };

struct CubeLut;
struct CurveTables;
struct BwParams;
struct ToningParams;
struct DuotoneParams;

// Potok (kolejnosc jak w specyfikacji): krzywe, [efekt testowy], czern-biel / sepia / monochromia, duotone, LUT, toning,
// na koncu sila ogolna (mieszanie z oryginalem). Modul z nullptr albo nieaktywny jest pomijany.
struct PipelineOptions {
    TestEffect effect = TestEffect::SCurve;
    float strength = 1.0f;  // 0..1, mieszanie z oryginalem
    bool dither = true;
    const CurveTables* curves = nullptr;
    const BwParams* bw = nullptr;
    const DuotoneParams* duotone = nullptr;
    const int32_t* duotoneMix = nullptr;  // mikser cz-b (6 wartosci) dla szarosci duotonu
    const CubeLut* lut = nullptr;
    float lutStrength = 1.0f;  // 0..1
    bool lutTrilinear = false;  // false = tetraedralnie
    const ToningParams* toning = nullptr;
    float overall = 1.0f;  // sila ogolna 0..1
};

// Przetwarza jeden piksel RGB (wartosci 0..1, tak jak je daje host).
void ProcessPixel(const PipelineOptions& o, const float in[3], float out[3]);

// Szum ditheringu w zakresie (-0.49, 0.49) LSB, zalezny wylacznie od bezwzglednych wspolrzednych i kanalu.
// Modul < 0.5 gwarantuje, ze wartosc niezmieniona przez efekt po zaokragleniu zostaje identyczna co do bitu.
float DitherNoise(int64_t x, int64_t y, int channel);

// Przetwarza wiersz przeplecionych pikseli (planes = 3 lub 4; plaszczyzna 3 to przezroczystosc).
// bps: 1 (8 bit, 0..255) albo 2 (16 bit, 0..32768). absX/absY: wspolrzedne pierwszego piksela w dokumencie.
// mask (opcjonalnie): 0..255 na piksel, 255 = w pelni zaznaczone.
void ProcessRow(const PipelineOptions& o, const uint8_t* in, uint8_t* out, int width, int planes, int bps,
                int64_t absX, int64_t absY, const uint8_t* mask);

// Wykonuje fn(y) dla y = 0..height-1 na kilku watkach (threads <= 0 = automatycznie, najwyzej 64).
// Odporne na brak zasobow: gdy nie da sie utworzyc watkow, wiersze wykonuje biezacy watek.
void ParallelRows(int height, int threads, const std::function<void(int)>& fn);

// Wielowatkowe przetworzenie prostokata (wiersze rozdzielone miedzy watki). threads <= 0 = automatycznie.
void ProcessRect(const PipelineOptions& o, const uint8_t* in, int inRowBytes, uint8_t* out, int outRowBytes,
                 int width, int height, int planes, int bps, int64_t absX, int64_t absY, const uint8_t* mask,
                 int maskRowBytes, int threads);

}  // namespace zk
