// Krzywe punktowe: RGB (wspolna) + R, G, B. Interpolacja monotoniczna szescienna (PCHIP, Fritsch-Carlson),
// bez przerzutow ponad punkty. Wartosci 0..1 na danych zakodowanych gamma (tak jak daje host).
#pragma once
#include <cstdint>
#include <vector>

namespace zk {

constexpr int kMaxCurvePoints = 16;
constexpr float kMinPointGap = 1.0f / 255.0f;  // minimalny odstep punktow w poziomie

struct CurvePoint {
    float x, y;
};

// POD: trzymane bezposrednio w parametrach filtra.
struct Curve {
    int32_t n = 2;
    CurvePoint p[kMaxCurvePoints] = {{0.0f, 0.0f}, {1.0f, 1.0f}};
};

enum CurveChannel { kCurveRGB = 0, kCurveR = 1, kCurveG = 2, kCurveB = 3, kCurveCount = 4 };

struct CurveSet {
    Curve c[kCurveCount];
    int32_t lumaOnly = 0;  // 1 = krzywa RGB zmienia tylko jasnosc (przyblizenie trybu Luminosity)
};

void ResetCurve(Curve& c);
bool IsIdentity(const Curve& c);
// Porzadkuje krzywa: 2..16 punktow, rosnace x z odstepem >= kMinPointGap, konce w x = 0 i x = 1, y w 0..1.
void SanitizeCurve(Curve& c);
// Wartosc krzywej w punkcie x (0..1), wynik przyciety do 0..1.
float EvalCurve(const Curve& c, float x);

// Tablice do szybkiego stosowania (probkowanie krzywej + interpolacja liniowa miedzy probkami).
struct CurveTables {
    static constexpr int kSize = 4096;
    bool active = false;          // false = wszystkie krzywe tozsamosciowe (piksel bez zmian, co do bitu)
    bool lumaOnly = false;
    bool used[kCurveCount] = {false, false, false, false};
    std::vector<float> t[kCurveCount];
};
void BuildCurveTables(const CurveSet& s, CurveTables& out);
// Kolejnosc jak w Photoshopie: krzywa RGB, potem krzywe kanalow.
void ApplyCurves(const CurveTables& t, float rgb[3]);

// Presety wbudowane (wlasne ksztalty autora).
enum class CurvePreset : int {
    SoftS = 0, MediumS, StrongS, Matte, LiftShadows, TameHighlights, CrossProcess, Negative, Count
};
void ApplyCurvePreset(CurvePreset preset, CurveSet& s);

}  // namespace zk
