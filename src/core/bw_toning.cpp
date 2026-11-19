#include "core/bw_toning.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float Luma(const float c[3]) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }
inline int32_t ClampI(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Barwi szarosc y kolorem o odcieniu hue: przesuniecie chromatyczne (kolor minus jego luminancja), wygaszane
// ku czerni i bieli (4y(1-y)), wiec czern i biel pozostaja neutralne, a luminancja tonow srednich - zachowana.
void Tint(float hueDeg, float amount, float y, float out[3], float weight) {
    float c[3];
    HueColor(hueDeg, c);
    const float yc = Luma(c);
    const float taper = 4.0f * y * (1.0f - y);
    for (int k = 0; k < 3; ++k) out[k] += (c[k] - yc) * amount * taper * weight;
}

inline float SmoothStep(float e0, float e1, float x) {
    float t = Clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

void HueColor(float hueDeg, float rgb[3]) {
    float h = std::fmod(hueDeg, 360.0f);
    if (h < 0) h += 360.0f;
    h /= 60.0f;
    const int i = (int)h;
    const float f = h - i;
    switch (i % 6) {
        case 0: rgb[0] = 1; rgb[1] = f; rgb[2] = 0; break;
        case 1: rgb[0] = 1 - f; rgb[1] = 1; rgb[2] = 0; break;
        case 2: rgb[0] = 0; rgb[1] = 1; rgb[2] = f; break;
        case 3: rgb[0] = 0; rgb[1] = 1 - f; rgb[2] = 1; break;
        case 4: rgb[0] = f; rgb[1] = 0; rgb[2] = 1; break;
        default: rgb[0] = 1; rgb[1] = 0; rgb[2] = 1 - f; break;
    }
}

void SanitizeBw(BwParams& p) {
    if (p.mode < 0 || p.mode >= (int32_t)BwMode::Count) p.mode = 0;
    for (auto& m : p.mix) m = ClampI(m, -200, 300);
    p.tintHue = ClampI(p.tintHue, 0, 360);
    p.tintSat = ClampI(p.tintSat, 0, 100);
}

void SanitizeToning(ToningParams& p) {
    p.on = p.on ? 1 : 0;
    p.hiHue = ClampI(p.hiHue, 0, 360);
    p.shHue = ClampI(p.shHue, 0, 360);
    p.hiSat = ClampI(p.hiSat, 0, 100);
    p.shSat = ClampI(p.shSat, 0, 100);
    p.balance = ClampI(p.balance, -100, 100);
}

bool BwActive(const BwParams& p) { return p.mode > 0 && p.mode < (int32_t)BwMode::Count; }
bool ToningActive(const ToningParams& p) { return p.on && (p.hiSat > 0 || p.shSat > 0); }

float BwGray(const int32_t mix[kBwColorCount], const float rgb[3]) {
    // Indeksy kanalow posortowane: imax, imid, imin.
    int imax = 0, imin = 0;
    for (int c = 1; c < 3; ++c) {
        if (rgb[c] > rgb[imax]) imax = c;
        if (rgb[c] < rgb[imin]) imin = c;
    }
    if (imax == imin) return rgb[0];  // szarosc neutralna
    const int imid = 3 - imax - imin;
    // Kolor podstawowy: kanal max (R, G, B). Kolor wtorny: mieszanka max + mid (Y = R+G, C = G+B, M = R+B).
    static const int kPrimary[3] = {kBwRed, kBwGreen, kBwBlue};
    auto secondary = [](int a, int b) {
        const int s = (1 << a) | (1 << b);
        return s == 3 ? kBwYellow : (s == 6 ? kBwCyan : kBwMagenta);
    };
    const float wp = mix[kPrimary[imax]] / 100.0f;
    const float ws = mix[secondary(imax, imid)] / 100.0f;
    const float g = rgb[imin] + (rgb[imid] - rgb[imin]) * ws + (rgb[imax] - rgb[imid]) * wp;
    return Clamp01(g);
}

void ApplyBw(const BwParams& p, float rgb[3]) {
    if (!BwActive(p)) return;
    const float y = BwGray(p.mix, rgb);
    rgb[0] = rgb[1] = rgb[2] = y;
    if (p.mode == (int32_t)BwMode::Sepia || p.mode == (int32_t)BwMode::Monochrome) {
        const float hue = p.mode == (int32_t)BwMode::Sepia ? (float)kSepiaHue : (float)p.tintHue;
        Tint(hue, p.tintSat / 100.0f, y, rgb, 1.0f);
        for (int k = 0; k < 3; ++k) rgb[k] = Clamp01(rgb[k]);
    }
}

void ApplyToning(const ToningParams& p, float rgb[3]) {
    if (!ToningActive(p)) return;
    const float y = Clamp01(Luma(rgb));
    // Punkt podzialu przesuwany balansem: 0.5 -> 0.25..0.75. Wagi swiatel/cieni przechodza plynnie (smoothstep).
    const float split = 0.5f - p.balance / 400.0f;
    const float wh = SmoothStep(split - 0.25f, split + 0.25f, y);
    // Toning stosuje sie po LUT-cie, na kolorowym obrazie - przesuniecie chromatyczne dodaje sie do koloru.
    if (p.hiSat > 0) Tint((float)p.hiHue, p.hiSat / 100.0f * 0.6f, y, rgb, wh);
    if (p.shSat > 0) Tint((float)p.shHue, p.shSat / 100.0f * 0.6f, y, rgb, 1.0f - wh);
    for (int k = 0; k < 3; ++k) rgb[k] = Clamp01(rgb[k]);
}

void SanitizeDuotone(DuotoneParams& p) {
    p.on = p.on ? 1 : 0;
    p.shadow &= 0xFFFFFF;
    p.highlight &= 0xFFFFFF;
    p.midpoint = ClampI(p.midpoint, 5, 95);
    p.amount = ClampI(p.amount, 0, 100);
}

bool DuotoneActive(const DuotoneParams& p) { return p.on && p.amount > 0; }

void ApplyDuotone(const DuotoneParams& p, const int32_t mix[kBwColorCount], float rgb[3]) {
    if (!DuotoneActive(p)) return;
    float t = BwGray(mix, rgb);
    // Punkt srodkowy: krzywa potegowa, ktora dla t = midpoint daje 0.5 (dla 50% - przejscie liniowe).
    const float mid = p.midpoint / 100.0f;
    if (p.midpoint != 50 && t > 0.0f && t < 1.0f) t = std::pow(t, std::log(0.5f) / std::log(mid));
    const float a = p.amount / 100.0f;
    for (int k = 0; k < 3; ++k) {
        const int sh = 16 - 8 * k;
        const float c0 = ((p.shadow >> sh) & 0xFF) / 255.0f, c1 = ((p.highlight >> sh) & 0xFF) / 255.0f;
        const float d = c0 + (c1 - c0) * t;
        rgb[k] = Clamp01(rgb[k] + (d - rgb[k]) * a);
    }
}

void ApplyDuotonePreset(DuotonePreset d, DuotoneParams& p) {
    static const uint32_t kColors[(int)DuotonePreset::Count][2] = {
        {0x17213D, 0xF2A65A},  // szybki start
        {0x101A3A, 0xFF9A3D},  // granatowy / pomaranczowy
        {0x12352B, 0xF2C14E},  // ciemna zielen / zloty
        {0x24113F, 0xFF5C8A},  // fiolet / roz
        {0x2A1710, 0xF5D6A1},  // ciemny braz / kremowy
        {0x000000, 0x35D6D0},  // czarny / turkus
    };
    const int i = (int)d >= 0 && (int)d < (int)DuotonePreset::Count ? (int)d : 0;
    p.on = 1;
    p.shadow = kColors[i][0];
    p.highlight = kColors[i][1];
    p.midpoint = i == 0 ? 48 : 50;
}

void ApplyBwFilter(BwFilter f, BwParams& p) {
    static const int32_t kMix[(int)BwFilter::Count][kBwColorCount] = {
        {40, 60, 40, 60, 20, 80},       // domyslny
        {120, 110, -10, -50, -50, 120},  // czerwony: ciemne niebo, jasna skora
        {110, 130, 10, -30, -40, 70},    // pomaranczowy
        {70, 140, 60, 0, -10, 40},       // zolty: delikatnie ciemniejsze niebo
        {-30, 80, 140, 60, -20, -10},    // zielony: jasna zielen, ciemniejsza skora
        {-20, 0, 20, 110, 140, 90},      // niebieski: jasne niebo, ciemne czerwienie
        {150, 160, -40, -60, -80, 120},  // wysoki kontrast
    };
    const int i = (int)f >= 0 && (int)f < (int)BwFilter::Count ? (int)f : 0;
    for (int k = 0; k < kBwColorCount; ++k) p.mix[k] = kMix[i][k];
}

void ApplyToningPreset(ToningPreset t, ToningParams& p) {
    p.on = 1;
    switch (t) {
        case ToningPreset::Classic: p.hiHue = 40; p.hiSat = 25; p.shHue = 215; p.shSat = 25; p.balance = 0; break;
        case ToningPreset::TealOrange: p.hiHue = 30; p.hiSat = 35; p.shHue = 190; p.shSat = 40; p.balance = 10; break;
        case ToningPreset::Selenium: p.hiHue = 30; p.hiSat = 8; p.shHue = 285; p.shSat = 30; p.balance = -20; break;
        case ToningPreset::Gold: p.hiHue = 45; p.hiSat = 35; p.shHue = 35; p.shSat = 15; p.balance = 20; break;
        case ToningPreset::Cold: p.hiHue = 200; p.hiSat = 15; p.shHue = 225; p.shSat = 35; p.balance = -10; break;
        default: break;
    }
}

}  // namespace zk
