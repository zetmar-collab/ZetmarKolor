// Czern-biel (mikser 6 kolorow), sepia, monochromia (barwienie jednym kolorem) i toning (swiatla / cienie).
// Wartosci 0..1 na danych zakodowanych gamma. Bez zaleznosci od Photoshopa.
#pragma once
#include <cstdint>

namespace zk {

enum class BwMode : int32_t { Off = 0, BlackWhite = 1, Sepia = 2, Monochrome = 3, Count };

// Kolejnosc suwakow miksera.
enum BwColor { kBwRed = 0, kBwYellow, kBwGreen, kBwCyan, kBwBlue, kBwMagenta, kBwColorCount };

// POD: trzymane bezposrednio w parametrach filtra.
struct BwParams {
    int32_t mode = 0;                                  // BwMode
    int32_t mix[kBwColorCount] = {40, 60, 40, 60, 20, 80};  // % (-200..300), domyslne jak w Photoshopie
    int32_t tintHue = 35;                              // 0..360 (sepia: staly odcien kSepiaHue)
    int32_t tintSat = 35;                              // 0..100
};

struct ToningParams {
    int32_t on = 0;
    int32_t hiHue = 40, hiSat = 0;    // swiatla: odcien 0..360, nasycenie 0..100
    int32_t shHue = 210, shSat = 0;   // cienie
    int32_t balance = 0;              // -100..100 (dodatni = wiecej swiatel)
};

constexpr int32_t kSepiaHue = 35;

void SanitizeBw(BwParams& p);
void SanitizeToning(ToningParams& p);

// true, gdy modul zmienia obraz (inaczej mozna go pominac - wynik identyczny co do bitu).
bool BwActive(const BwParams& p);
bool ToningActive(const ToningParams& p);

// Szarosc z miksera: min + (mid - min) * w(kolor wtorny) + (max - mid) * w(kolor podstawowy).
// Szarosc neutralna (R = G = B) zostaje bez zmian przy kazdych ustawieniach.
float BwGray(const int32_t mix[kBwColorCount], const float rgb[3]);
void ApplyBw(const BwParams& p, float rgb[3]);
void ApplyToning(const ToningParams& p, float rgb[3]);

// Kolor czysty o danym odcieniu (HSL: S = 100%, L = 50%).
void HueColor(float hueDeg, float rgb[3]);

// Presety "kolor filtru" dla miksera (wlasne wartosci autora).
enum class BwFilter : int { Default = 0, Red, Orange, Yellow, Green, Blue, HighContrast, Count };
void ApplyBwFilter(BwFilter f, BwParams& p);

enum class ToningPreset : int { Classic = 0, TealOrange, Selenium, Gold, Cold, Count };
void ApplyToningPreset(ToningPreset t, ToningParams& p);

// Duotone (mapa gradientu): szarosc z miksera cz-b przypisana do przejscia kolor cieni -> kolor swiatel.
struct DuotoneParams {
    int32_t on = 0;
    uint32_t shadow = 0x17213D;     // 0xRRGGBB (wartosci zakodowane, jak w mapie gradientu Photoshopa)
    uint32_t highlight = 0xF2A65A;
    int32_t midpoint = 48;          // 5..95 %: gdzie wypada polowa przejscia (w lewo = wiecej koloru swiatel)
    int32_t amount = 100;           // 0..100 %: mieszanie z obrazem wejsciowym
};
void SanitizeDuotone(DuotoneParams& p);
bool DuotoneActive(const DuotoneParams& p);
// mix: suwaki miksera cz-b (szarosc liczona jak w zakladce Cz-B).
void ApplyDuotone(const DuotoneParams& p, const int32_t mix[kBwColorCount], float rgb[3]);

// Zestawy kolorow: szybki start, granat/pomarancz, zielen/zloto, fiolet/roz, braz/krem, czern/turkus.
enum class DuotonePreset : int { QuickStart = 0, NavyOrange, GreenGold, PurplePink, BrownCream, BlackTeal, Count };
void ApplyDuotonePreset(DuotonePreset d, DuotoneParams& p);

}  // namespace zk
