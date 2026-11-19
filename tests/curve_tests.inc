// Testy krzywych (dolaczane do core_tests.cpp).
#include "core/curves.h"

static void RunCurveTests() {
    // --- Krzywe tozsamosciowe: wynik identyczny co do bitu (8/16 bit, z ditheringiem).
    {
        CurveSet s;
        CurveTables t;
        BuildCurveTables(s, t);
        CHECK(!t.active, "krzywe: domyslny zestaw jest tozsamosciowy (nieaktywny)");
        // Punkty dodane na przekatnej nadal daja tozsamosc.
        s.c[kCurveRGB].n = 4;
        s.c[kCurveRGB].p[0] = {0, 0}; s.c[kCurveRGB].p[1] = {0.3f, 0.3f};
        s.c[kCurveRGB].p[2] = {0.7f, 0.7f}; s.c[kCurveRGB].p[3] = {1, 1};
        BuildCurveTables(s, t);
        for (int bps = 1; bps <= 2; ++bps) {
            Img im = MakeImg(211, 97, 4, bps, 31 + bps);
            PipelineOptions o;
            o.effect = TestEffect::None;
            o.curves = &t;
            o.dither = true;
            char m[96];
            std::snprintf(m, sizeof m, "krzywe: punkty na przekatnej = obraz identyczny co do bitu (%d bit)", bps * 8);
            CHECK(Whole(o, im, 4) == im.d, m);
        }
    }

    // --- Monotonicznosc i brak przerzutow na losowych krzywych rosnacych.
    {
        std::mt19937 rng(7);
        bool mono = true, within = true;
        for (int k = 0; k < 500; ++k) {
            Curve c;
            c.n = 2 + (int)(rng() % (kMaxCurvePoints - 1));
            std::vector<float> xs(c.n), ys(c.n);
            for (int i = 0; i < c.n; ++i) {
                xs[i] = (rng() % 10000) / 10000.0f;
                ys[i] = (rng() % 10000) / 10000.0f;
            }
            std::sort(xs.begin(), xs.end());
            std::sort(ys.begin(), ys.end());
            for (int i = 0; i < c.n; ++i) c.p[i] = {xs[i], ys[i]};
            SanitizeCurve(c);
            float prev = -1.0f;
            for (int i = 0; i <= 4096; ++i) {
                float x = i / 4096.0f, y = EvalCurve(c, x);
                if (y < prev - 1e-6f) mono = false;
                prev = y;
                // Brak przerzutu: wartosc w przedziale miedzy sasiednimi punktami.
                int s = 0;
                while (s < c.n - 2 && x > c.p[s + 1].x) ++s;
                float lo = std::min(c.p[s].y, c.p[s + 1].y), hi = std::max(c.p[s].y, c.p[s + 1].y);
                if (y < lo - 1e-5f || y > hi + 1e-5f) within = false;
            }
        }
        CHECK(mono, "krzywe: 500 losowych krzywych rosnacych pozostaje monotonicznych (brak odwrocen tonow)");
        CHECK(within, "krzywe: brak przerzutow ponad punkty (PCHIP)");
    }

    // --- Krzywa przechodzi przez swoje punkty; S-krzywa przyciemnia cienie i rozjasnia swiatla.
    {
        CurveSet s;
        ApplyCurvePreset(CurvePreset::MediumS, s);
        const Curve& c = s.c[kCurveRGB];
        bool through = true;
        for (int i = 0; i < c.n; ++i)
            if (std::fabs(EvalCurve(c, c.p[i].x) - c.p[i].y) > 1e-5f) through = false;
        CHECK(through, "krzywe: krzywa przechodzi dokladnie przez punkty");
        CHECK(EvalCurve(c, 0.25f) < 0.25f && EvalCurve(c, 0.75f) > 0.75f && std::fabs(EvalCurve(c, 0.5f) - 0.5f) < 0.01f,
              "krzywe: preset S-krzywa sredniej sily ma oczekiwany ksztalt");
        CurveSet neg;
        ApplyCurvePreset(CurvePreset::Negative, neg);
        CHECK(std::fabs(EvalCurve(neg.c[kCurveRGB], 0.3f) - 0.7f) < 1e-5f, "krzywe: preset negatyw daje 1-x");
        bool allOk = true;
        for (int pr = 0; pr < (int)CurvePreset::Count; ++pr) {
            CurveSet t;
            ApplyCurvePreset((CurvePreset)pr, t);
            CurveTables tt;
            BuildCurveTables(t, tt);
            if (!tt.active) allOk = false;
        }
        CHECK(allOk, "krzywe: kazdy preset zmienia obraz");
    }

    // --- Kolejnosc: krzywa RGB, potem kanal (jak w Photoshopie).
    {
        CurveSet s;
        s.c[kCurveRGB].p[1].y = 1.0f;  // RGB: y = 1 dla x = 1, punkt (0, 0.5)
        s.c[kCurveRGB].p[0].y = 0.5f;  // RGB: y = 0.5 + 0.5x
        s.c[kCurveR].p[0].y = 1.0f;    // R: y = 1 - x
        s.c[kCurveR].p[1].y = 0.0f;
        CurveTables t;
        BuildCurveTables(s, t);
        float v[3] = {0.0f, 0.0f, 0.0f};
        ApplyCurves(t, v);
        CHECK(std::fabs(v[0] - 0.5f) < 1e-3f && std::fabs(v[1] - 0.5f) < 1e-3f,
              "krzywe: najpierw RGB (0 -> 0,5), potem R (0,5 -> 0,5)");
    }

    // --- Tryb "tylko jasnosc": rowne kanaly (szarosc) zostaja szare, kolor przesuwa sie o te sama wartosc.
    {
        CurveSet s;
        ApplyCurvePreset(CurvePreset::LiftShadows, s);
        s.lumaOnly = 1;
        CurveTables t;
        BuildCurveTables(s, t);
        float g[3] = {0.3f, 0.3f, 0.3f};
        ApplyCurves(t, g);
        float c[3] = {0.5f, 0.2f, 0.1f};
        float y = 0.2126f * 0.5f + 0.7152f * 0.2f + 0.0722f * 0.1f;
        float dy = EvalCurve(s.c[kCurveRGB], y) - y;
        ApplyCurves(t, c);
        CHECK(std::fabs(g[0] - g[1]) < 1e-6f && std::fabs(g[1] - g[2]) < 1e-6f && g[0] > 0.3f,
              "krzywe: tylko jasnosc - szarosc pozostaje szara i jasnieje");
        CHECK(std::fabs((c[0] - 0.5f) - dy) < 2e-3f && std::fabs((c[2] - 0.1f) - dy) < 2e-3f,
              "krzywe: tylko jasnosc - RGB' = RGB + (Y' - Y)");
    }

    // --- Porzadkowanie punktow (dane z uchwytu parametrow moga byc dowolne).
    {
        Curve c;
        c.n = 5;
        c.p[0] = {0.9f, 0.1f}; c.p[1] = {0.2f, 0.5f}; c.p[2] = {0.2001f, 0.6f}; c.p[3] = {0.05f, 2.0f}; c.p[4] = {0.5f, -1.0f};
        SanitizeCurve(c);
        bool sorted = c.p[0].x == 0.0f && c.p[c.n - 1].x == 1.0f;
        for (int i = 1; i < c.n; ++i)
            if (c.p[i].x - c.p[i - 1].x < kMinPointGap - 1e-7f) sorted = false;
        for (int i = 0; i < c.n; ++i)
            if (c.p[i].y < 0.0f || c.p[i].y > 1.0f) sorted = false;
        CHECK(sorted, "krzywe: SanitizeCurve sortuje, usuwa zbyt bliskie punkty, przycina wartosci");
        Curve bad;
        bad.n = 99;
        SanitizeCurve(bad);
        CHECK(bad.n == 2 && IsIdentity(bad), "krzywe: niepoprawna liczba punktow -> krzywa domyslna");
        Curve nan;
        nan.p[0].y = std::nanf("");
        SanitizeCurve(nan);
        CHECK(IsIdentity(nan), "krzywe: NaN w punktach -> krzywa domyslna");
    }

    // --- Krzywe + kafelki/watki: wynik powtarzalny.
    {
        CurveSet s;
        ApplyCurvePreset(CurvePreset::CrossProcess, s);
        CurveTables t;
        BuildCurveTables(s, t);
        Img im = MakeImg(301, 177, 4, 2, 11);
        PipelineOptions o;
        o.effect = TestEffect::None;
        o.curves = &t;
        auto ref = Whole(o, im, 1);
        CHECK(Tiled(o, im, 37, 53, 3) == ref && Whole(o, im, 8) == ref, "krzywe: wynik niezalezny od kafelkow i watkow");
    }
}
