#include "core/curves.h"

#include <algorithm>
#include <cmath>

namespace zk {

namespace {

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Pochodne w wezlach wg Fritscha-Carlsona (srednia harmoniczna wazona, zero przy zmianie kierunku).
void Slopes(const Curve& c, float* m) {
    const int n = c.n;
    float d[kMaxCurvePoints], h[kMaxCurvePoints];
    for (int k = 0; k < n - 1; ++k) {
        h[k] = c.p[k + 1].x - c.p[k].x;
        d[k] = (c.p[k + 1].y - c.p[k].y) / h[k];
    }
    if (n == 2) { m[0] = m[1] = d[0]; return; }
    for (int k = 1; k < n - 1; ++k) {
        if (d[k - 1] * d[k] <= 0.0f) { m[k] = 0.0f; continue; }
        const float w1 = 2.0f * h[k] + h[k - 1], w2 = h[k] + 2.0f * h[k - 1];
        m[k] = (w1 + w2) / (w1 / d[k - 1] + w2 / d[k]);
    }
    // Konce: wzor trzypunktowy z ograniczeniem zachowujacym ksztalt (jak pchip w MATLAB/SciPy).
    auto endSlope = [](float h0, float h1, float d0, float d1) {
        float s = ((2.0f * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
        if (s * d0 <= 0.0f) return 0.0f;
        if (d0 * d1 <= 0.0f && std::fabs(s) > std::fabs(3.0f * d0)) return 3.0f * d0;
        return s;
    };
    m[0] = endSlope(h[0], h[1], d[0], d[1]);
    m[n - 1] = endSlope(h[n - 2], h[n - 3], d[n - 2], d[n - 3]);
}

float EvalWithSlopes(const Curve& c, const float* m, float x) {
    const int n = c.n;
    if (x <= c.p[0].x) return Clamp01(c.p[0].y);
    if (x >= c.p[n - 1].x) return Clamp01(c.p[n - 1].y);
    int k = 0;
    while (k < n - 2 && x > c.p[k + 1].x) ++k;
    const float h = c.p[k + 1].x - c.p[k].x;
    const float t = (x - c.p[k].x) / h, t2 = t * t, t3 = t2 * t;
    const float h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
    return Clamp01(h00 * c.p[k].y + h10 * h * m[k] + h01 * c.p[k + 1].y + h11 * h * m[k + 1]);
}

inline float Lookup(const std::vector<float>& t, float v) {
    float f = Clamp01(v) * (CurveTables::kSize - 1);
    int i = (int)f;
    if (i >= CurveTables::kSize - 1) return t[CurveTables::kSize - 1];
    float a = f - i;
    return t[i] + (t[i + 1] - t[i]) * a;
}

void SetPoints(Curve& c, std::initializer_list<CurvePoint> pts) {
    c.n = 0;
    for (const auto& p : pts) c.p[c.n++] = p;
    SanitizeCurve(c);
}

}  // namespace

void ResetCurve(Curve& c) { c = Curve(); }

bool IsIdentity(const Curve& c) {
    if (c.n < 2) return true;
    for (int i = 0; i < c.n; ++i)
        if (std::fabs(c.p[i].x - c.p[i].y) > 1e-6f) return false;
    return true;  // punkty na przekatnej => PCHIP daje dokladnie prosta
}

void SanitizeCurve(Curve& c) {
    if (c.n < 2 || c.n > kMaxCurvePoints) { ResetCurve(c); return; }
    for (int i = 0; i < c.n; ++i) {
        if (!std::isfinite(c.p[i].x) || !std::isfinite(c.p[i].y)) { ResetCurve(c); return; }
        c.p[i].x = Clamp01(c.p[i].x);
        c.p[i].y = Clamp01(c.p[i].y);
    }
    std::sort(c.p, c.p + c.n, [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
    // Konce zawsze w x = 0 i x = 1 (mozna je przesuwac tylko w pionie).
    c.p[0].x = 0.0f;
    c.p[c.n - 1].x = 1.0f;
    // Usuwanie punktow zbyt blisko poprzednika (konce zostaja).
    int w = 1;
    for (int i = 1; i < c.n - 1; ++i) {
        if (c.p[i].x - c.p[w - 1].x >= kMinPointGap && 1.0f - c.p[i].x >= kMinPointGap) c.p[w++] = c.p[i];
    }
    c.p[w++] = c.p[c.n - 1];
    c.n = w;
}

float EvalCurve(const Curve& c, float x) {
    float m[kMaxCurvePoints];
    Slopes(c, m);
    return EvalWithSlopes(c, m, x);
}

void BuildCurveTables(const CurveSet& s, CurveTables& out) {
    out.active = false;
    out.lumaOnly = s.lumaOnly != 0;
    for (int ch = 0; ch < kCurveCount; ++ch) {
        const Curve& c = s.c[ch];
        out.used[ch] = !IsIdentity(c);
        if (!out.used[ch]) { out.t[ch].clear(); continue; }
        out.active = true;
        float m[kMaxCurvePoints];
        Slopes(c, m);
        out.t[ch].resize(CurveTables::kSize);
        for (int i = 0; i < CurveTables::kSize; ++i)
            out.t[ch][i] = EvalWithSlopes(c, m, i / (float)(CurveTables::kSize - 1));
    }
}

void ApplyCurves(const CurveTables& t, float rgb[3]) {
    if (!t.active) return;
    if (t.used[kCurveRGB]) {
        if (t.lumaOnly) {
            // Tryb "tylko jasnosc": Y wg Rec. 709 na wartosciach zakodowanych, RGB' = RGB + (Y' - Y).
            // Przyblizenie trybu mieszania Luminosity (nasycenie przy duzych zmianach moze sie lekko zmienic).
            const float y = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
            const float dy = Lookup(t.t[kCurveRGB], y) - y;
            for (int c = 0; c < 3; ++c) rgb[c] = Clamp01(rgb[c] + dy);
        } else {
            for (int c = 0; c < 3; ++c) rgb[c] = Lookup(t.t[kCurveRGB], rgb[c]);
        }
    }
    for (int c = 0; c < 3; ++c)
        if (t.used[kCurveR + c]) rgb[c] = Lookup(t.t[kCurveR + c], rgb[c]);
}

void ApplyCurvePreset(CurvePreset preset, CurveSet& s) {
    for (auto& c : s.c) ResetCurve(c);
    Curve& m = s.c[kCurveRGB];
    switch (preset) {
        case CurvePreset::SoftS: SetPoints(m, {{0, 0}, {0.25f, 0.22f}, {0.75f, 0.78f}, {1, 1}}); break;
        case CurvePreset::MediumS: SetPoints(m, {{0, 0}, {0.25f, 0.19f}, {0.75f, 0.81f}, {1, 1}}); break;
        case CurvePreset::StrongS: SetPoints(m, {{0, 0}, {0.25f, 0.15f}, {0.75f, 0.86f}, {1, 1}}); break;
        case CurvePreset::Matte: SetPoints(m, {{0, 0.08f}, {0.25f, 0.27f}, {0.75f, 0.76f}, {1, 0.96f}}); break;
        case CurvePreset::LiftShadows: SetPoints(m, {{0, 0}, {0.22f, 0.31f}, {0.55f, 0.6f}, {1, 1}}); break;
        case CurvePreset::TameHighlights: SetPoints(m, {{0, 0}, {0.5f, 0.5f}, {0.8f, 0.74f}, {1, 0.94f}}); break;
        case CurvePreset::CrossProcess:
            SetPoints(s.c[kCurveR], {{0, 0}, {0.25f, 0.19f}, {0.75f, 0.83f}, {1, 1}});
            SetPoints(s.c[kCurveG], {{0, 0}, {0.25f, 0.21f}, {0.75f, 0.8f}, {1, 1}});
            SetPoints(s.c[kCurveB], {{0, 0.12f}, {0.5f, 0.47f}, {1, 0.86f}});
            break;
        case CurvePreset::Negative: SetPoints(m, {{0, 1}, {1, 0}}); break;
        default: break;
    }
}

}  // namespace zk
