// Testy czerni-bieli, sepii, monochromii, tonowania i sily ogolnej (dolaczane do core_tests.cpp).
#include "core/bw_toning.h"

static void RunBwTests() {
    // --- Szarosc neutralna nie zmienia sie przy zadnym ustawieniu miksera.
    {
        bool ok = true;
        std::mt19937 rng(3);
        for (int k = 0; k < 200; ++k) {
            int32_t mix[kBwColorCount];
            for (auto& m : mix) m = -200 + (int)(rng() % 501);
            float v = (rng() % 1001) / 1000.0f, rgb[3] = {v, v, v};
            if (std::fabs(BwGray(mix, rgb) - v) > 1e-6f) ok = false;
        }
        CHECK(ok, "cz-b: szarosc neutralna bez zmian przy dowolnym mikserze");
    }
    // --- Wynik w trybie cz-b jest szary (R = G = B), czern i biel zostaja.
    {
        BwParams p;
        p.mode = (int)BwMode::BlackWhite;
        float c[3] = {0.8f, 0.3f, 0.1f};
        ApplyBw(p, c);
        float k[3] = {0, 0, 0}, w[3] = {1, 1, 1};
        ApplyBw(p, k);
        ApplyBw(p, w);
        CHECK(c[0] == c[1] && c[1] == c[2] && k[0] == 0.0f && w[0] == 1.0f, "cz-b: wynik szary, czern i biel bez zmian");
    }
    // --- Filtr czerwony rozjasnia czerwien i przyciemnia niebieski wzgledem domyslnego.
    {
        BwParams def, red;
        ApplyBwFilter(BwFilter::Red, red);
        float r1[3] = {0.8f, 0.2f, 0.2f}, r2[3] = {0.8f, 0.2f, 0.2f}, b1[3] = {0.2f, 0.3f, 0.8f}, b2[3] = {0.2f, 0.3f, 0.8f};
        CHECK(BwGray(red.mix, r1) > BwGray(def.mix, r2) && BwGray(red.mix, b1) < BwGray(def.mix, b2),
              "cz-b: filtr czerwony - jasniejsza czerwien, ciemniejszy niebieski");
    }
    // --- Mikser: suwak koloru dziala tylko na swoj kolor (czysty czerwony zalezy od suwaka Czerwony).
    {
        BwParams a, b;
        b.mix[kBwRed] = 200;
        float red[3] = {1, 0, 0}, red2[3] = {1, 0, 0}, grn[3] = {0, 1, 0}, grn2[3] = {0, 1, 0};
        CHECK(BwGray(b.mix, red) > BwGray(a.mix, red2) && BwGray(b.mix, grn) == BwGray(a.mix, grn2),
              "cz-b: suwak Czerwony zmienia czerwien, nie zmienia zieleni");
    }
    // --- Sepia: ciepla (R > G > B) w tonach srednich, neutralna w czerni i bieli.
    {
        BwParams p;
        p.mode = (int)BwMode::Sepia;
        p.tintSat = 50;
        float m[3] = {0.5f, 0.5f, 0.5f}, k[3] = {0, 0, 0}, w[3] = {1, 1, 1};
        ApplyBw(p, m);
        ApplyBw(p, k);
        ApplyBw(p, w);
        CHECK(m[0] > m[1] && m[1] > m[2] && k[0] == k[2] && w[0] == w[2], "sepia: cieple tony srednie, czern i biel neutralne");
        float y = 0.2126f * m[0] + 0.7152f * m[1] + 0.0722f * m[2];
        CHECK(std::fabs(y - 0.5f) < 0.01f, "sepia: luminancja tonow srednich zachowana");
    }
    // --- Monochromia: odcien 210 (niebieski) daje B > R.
    {
        BwParams p;
        p.mode = (int)BwMode::Monochrome;
        p.tintHue = 210;
        p.tintSat = 60;
        float m[3] = {0.4f, 0.4f, 0.4f};
        ApplyBw(p, m);
        CHECK(m[2] > m[0], "monochromia: odcien niebieski barwi na niebiesko");
    }
    // --- Toning: nieaktywny przy nasyceniu 0 (wynik co do bitu), cienie i swiatla barwione osobno.
    {
        ToningParams t;
        t.on = 1;
        CHECK(!ToningActive(t), "toning: nasycenie 0 = modul nieaktywny");
        ApplyToningPreset(ToningPreset::Classic, t);
        float dark[3] = {0.2f, 0.2f, 0.2f}, light[3] = {0.8f, 0.8f, 0.8f};
        ApplyToning(t, dark);
        ApplyToning(t, light);
        CHECK(dark[2] > dark[0] && light[0] > light[2], "toning klasyczny: zimne cienie, cieple swiatla");
        ToningParams b = t;
        b.balance = 100;
        float mid[3] = {0.45f, 0.45f, 0.45f}, mid2[3] = {0.45f, 0.45f, 0.45f};
        ApplyToning(t, mid);
        ApplyToning(b, mid2);
        CHECK(mid2[0] - mid2[2] > mid[0] - mid[2], "toning: balans +100 przesuwa tony srednie w strone swiatel");
    }
    // --- Zakresy po odczycie z uchwytu.
    {
        BwParams p;
        p.mode = 99; p.mix[0] = 9999; p.tintHue = -5; p.tintSat = 300;
        SanitizeBw(p);
        ToningParams t;
        t.balance = 500; t.hiSat = -1;
        SanitizeToning(t);
        CHECK(p.mode == 0 && p.mix[0] == 300 && p.tintHue == 0 && p.tintSat == 100 && t.balance == 100 && t.hiSat == 0,
              "cz-b / toning: wartosci spoza zakresu przyciete");
    }
    // --- Potok: sila ogolna 0 = obraz identyczny co do bitu; caly potok niezalezny od kafelkow.
    {
        BwParams bw;
        bw.mode = (int)BwMode::Sepia;
        ToningParams tn;
        ApplyToningPreset(ToningPreset::TealOrange, tn);
        for (int bps = 1; bps <= 2; ++bps) {
            Img im = MakeImg(173, 91, 4, bps, 50 + bps);
            PipelineOptions o;
            o.effect = TestEffect::None;
            o.bw = &bw;
            o.toning = &tn;
            o.overall = 0.0f;
            char m[96];
            std::snprintf(m, sizeof m, "potok: sila ogolna 0%% = obraz identyczny co do bitu (%d bit)", bps * 8);
            CHECK(Whole(o, im, 4) == im.d, m);
            o.overall = 0.7f;
            auto ref = Whole(o, im, 1);
            std::snprintf(m, sizeof m, "potok: cz-b + toning niezalezne od kafelkow i watkow (%d bit)", bps * 8);
            CHECK(Tiled(o, im, 37, 53, 3) == ref && Whole(o, im, 8) == ref, m);
        }
    }
}
