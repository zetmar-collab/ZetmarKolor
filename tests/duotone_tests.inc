// Testy duotonu (dolaczane do core_tests.cpp).

static void RunDuotoneTests() {
    BwParams bw;  // mikser domyslny
    DuotoneParams d;
    CHECK(!DuotoneActive(d), "duotone: domyslnie wylaczony");
    d.on = 1;
    d.shadow = 0x102030;
    d.highlight = 0xF0E0D0;
    d.midpoint = 50;
    auto Near255 = [](float a, int v) { return std::fabs(a * 255.0f - v) < 0.6f; };
    float k[3] = {0, 0, 0}, w[3] = {1, 1, 1}, m[3] = {0.5f, 0.5f, 0.5f};
    ApplyDuotone(d, bw.mix, k);
    ApplyDuotone(d, bw.mix, w);
    ApplyDuotone(d, bw.mix, m);
    CHECK(Near255(k[0], 0x10) && Near255(k[1], 0x20) && Near255(k[2], 0x30), "duotone: czern -> kolor cieni");
    CHECK(Near255(w[0], 0xF0) && Near255(w[1], 0xE0) && Near255(w[2], 0xD0), "duotone: biel -> kolor swiatel");
    CHECK(Near255(m[0], (0x10 + 0xF0) / 2) && Near255(m[2], (0x30 + 0xD0) / 2), "duotone: szarosc 50% -> polowa przejscia");
    // Punkt srodkowy 30%: szarosc 0.3 daje polowe przejscia, a wiecej obrazu dostaje kolor swiatel.
    d.midpoint = 30;
    float g3[3] = {0.3f, 0.3f, 0.3f}, g5[3] = {0.5f, 0.5f, 0.5f};
    ApplyDuotone(d, bw.mix, g3);
    ApplyDuotone(d, bw.mix, g5);
    CHECK(Near255(g3[0], (0x10 + 0xF0) / 2) && g5[0] > m[0], "duotone: punkt srodkowy w lewo = wiecej koloru swiatel");
    // Przejscie monotoniczne (brak odwrocen tonow).
    bool mono = true;
    float prev = -1;
    for (int i = 0; i <= 255; ++i) {
        float c[3] = {i / 255.0f, i / 255.0f, i / 255.0f};
        ApplyDuotone(d, bw.mix, c);
        if (c[0] < prev - 1e-6f) mono = false;
        prev = c[0];
    }
    CHECK(mono, "duotone: przejscie monotoniczne (jasniejsze wejscie = jasniejszy wynik)");
    // Sila 50%: polowa drogi miedzy wejsciem a duotonem.
    d.midpoint = 50;
    d.amount = 50;
    float c[3] = {0.0f, 0.0f, 0.0f};
    ApplyDuotone(d, bw.mix, c);
    CHECK(Near255(c[0], 0x10 / 2) && Near255(c[2], 0x30 / 2), "duotone: sila 50% miesza z obrazem wejsciowym");
    // Presety i zakresy.
    DuotoneParams q;
    ApplyDuotonePreset(DuotonePreset::BlackTeal, q);
    CHECK(q.on && q.shadow == 0x000000 && q.highlight == 0x35D6D0, "duotone: preset czarny / turkus");
    DuotoneParams bad;
    bad.midpoint = 0; bad.amount = 500; bad.shadow = 0xFF123456;
    SanitizeDuotone(bad);
    CHECK(bad.midpoint == 5 && bad.amount == 100 && bad.shadow == 0x123456, "duotone: wartosci spoza zakresu przyciete");
    // Potok: duotone z kafelkami i watkami, sila ogolna 0 = bez zmian.
    for (int bps = 1; bps <= 2; ++bps) {
        Img im = MakeImg(157, 83, 4, bps, 70 + bps);
        DuotoneParams dd;
        ApplyDuotonePreset(DuotonePreset::NavyOrange, dd);
        PipelineOptions o;
        o.effect = TestEffect::None;
        o.duotone = &dd;
        o.duotoneMix = bw.mix;
        auto ref = Whole(o, im, 1);
        char msg[96];
        std::snprintf(msg, sizeof msg, "duotone: wynik niezalezny od kafelkow i watkow (%d bit)", bps * 8);
        CHECK(Tiled(o, im, 37, 53, 3) == ref && Whole(o, im, 8) == ref, msg);
        o.overall = 0.0f;
        std::snprintf(msg, sizeof msg, "duotone: sila ogolna 0%% = obraz identyczny co do bitu (%d bit)", bps * 8);
        CHECK(Whole(o, im, 4) == im.d, msg);
    }
}
