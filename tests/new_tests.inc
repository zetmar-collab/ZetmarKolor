// Testy poprawek i nowych funkcji: ParallelRows, walidacja parametrow, presety Look, eksport .cube, zapis atomowy,
// parser (konce linii, '#' w TITLE), wezly LUT-a 3D (dolaczane do core_tests.cpp).
#include <functional>

#include "core/look_preset.h"
#include "core/pipl_binary.h"
#include "core/settings.h"
#include "plugin/host.h"

static bool Near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

static FilterParams RichLook() {
    FilterParams a;
    ApplyCurvePreset(CurvePreset::StrongS, a.curves);
    a.curves.lumaOnly = 1;
    a.bw.mode = (int)BwMode::Sepia;
    a.bw.mix[3] = -50;
    a.bw.tintHue = 120;
    a.bw.tintSat = 61;
    a.toning.on = 1;
    a.toning.hiHue = 33;
    a.toning.hiSat = 40;
    a.toning.shHue = 201;
    a.toning.shSat = 22;
    a.toning.balance = -17;
    a.duotone.on = 1;
    a.duotone.shadow = 0x123456;
    a.duotone.highlight = 0xFEDCBA;
    a.duotone.midpoint = 61;
    a.duotone.amount = 73;
    a.lutStrength = 44;
    a.lutTrilinear = 1;
    wcscpy_s(a.lutPath, L"C:\\Luty \u0141\u00F3d\u017A\\Ciep\u0142y.cube");
    a.overall = 60;
    return a;
}

static void RunNewTests() {
    // --- ParallelRows: kazdy wiersz dokladnie raz, niezaleznie od liczby watkow (takze absurdalnej).
    {
        bool ok = true;
        for (int t : {-3, 0, 1, 3, 64, 1000}) {
            std::vector<int> cnt(97, 0);
            ParallelRows(97, t, [&](int y) { ++cnt[(size_t)y]; });
            for (int c : cnt) ok = ok && c == 1;
        }
        ParallelRows(0, 4, [&](int) { ok = false; });
        CHECK(ok, "ParallelRows: kazdy wiersz raz dla 0, 1, 3, 64 i 1000 watkow");
    }

    // --- Walidacja parametrow filtra (uszkodzony uchwyt)
    {
        FilterParams p;
        CHECK(IsValid(p), "parametry domyslne sa poprawne");
        p.threads = -5;
        CHECK(!IsValid(p), "IsValid: ujemna liczba watkow odrzucona");
        SanitizeParams(p);
        CHECK(p.threads == 0 && IsValid(p), "SanitizeParams: watki przyciete do zakresu");
        p.threads = 100000;
        CHECK(!IsValid(p), "IsValid: absurdalna liczba watkow odrzucona");
        FilterParams q;
        for (auto& c : q.lutPath) c = L'x';
        CHECK(!IsValid(q), "IsValid: sciezka LUT bez zera konczacego odrzucona");
        SanitizeParams(q);
        CHECK(IsValid(q), "SanitizeParams: sciezka LUT zawsze zakonczona zerem");
        FilterParams r;
        r.overall = 900; r.lutStrength = -4; r.bw.mix[0] = 9999; r.toning.balance = 7000;
        r.curves.c[0].n = 99;
        SanitizeParams(r);
        CHECK(r.overall == 100 && r.lutStrength == 0 && r.bw.mix[0] == 300 && r.toning.balance == 100 &&
                  r.curves.c[0].n == 2,
              "SanitizeParams: wartosci spoza zakresu przyciete, zla krzywa wyzerowana");
        if (!DebugEnabled()) {
            FilterParams t;
            t.effect = 2; t.maskMode = 1; t.markTiles = 1; t.tileMode = 2; t.dither = 0; t.strength = 10;
            SanitizeParams(t);
            CHECK(t.effect == 0 && t.maskMode == 0 && t.markTiles == 0 && t.tileMode == 0 && t.dither == 1 &&
                      t.strength == 100,
                  "SanitizeParams: opcje testowe wylaczone poza trybem diagnostycznym");
        }
    }

    // --- Preset Look: zapis -> odczyt
    {
        FilterParams a = RichLook();
        const std::string text = LookToText(a);
        FilterParams b;
        b.effect = 0;
        const bool ok = LookFromText(text.data(), text.size(), b);
        bool same = ok && b.curvesOn == a.curvesOn && b.curves.lumaOnly == 1 && b.bw.mode == a.bw.mode &&
                    b.bw.mix[3] == -50 && b.bw.tintHue == 120 && b.bw.tintSat == 61 && b.toning.on == 1 &&
                    b.toning.hiHue == 33 && b.toning.hiSat == 40 && b.toning.shHue == 201 && b.toning.shSat == 22 &&
                    b.toning.balance == -17 && b.duotone.on == 1 && b.duotone.shadow == 0x123456 &&
                    b.duotone.highlight == 0xFEDCBA && b.duotone.midpoint == 61 && b.duotone.amount == 73 &&
                    b.lutStrength == 44 && b.lutTrilinear == 1 && wcscmp(b.lutPath, a.lutPath) == 0 && b.overall == 60;
        for (int ch = 0; ch < kCurveCount && same; ++ch) {
            same = b.curves.c[ch].n == a.curves.c[ch].n;
            for (int i = 0; same && i < a.curves.c[ch].n; ++i)
                same = Near(b.curves.c[ch].p[i].x, a.curves.c[ch].p[i].x, 1e-5f) &&
                       Near(b.curves.c[ch].p[i].y, a.curves.c[ch].p[i].y, 1e-5f);
        }
        CHECK(same, "look: zapis i odczyt zachowuje wszystkie pola (z polskimi znakami w sciezce LUT)");

        // Plik na dysku (zapis atomowy) i BOM / CRLF.
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        const std::wstring path = std::wstring(tmp) + L"zk_look_" + std::to_wstring(GetCurrentProcessId()) + L".zklook";
        FilterParams c;
        const bool fileOk = SaveLookFile(path, a) && LoadLookFile(path, c) && wcscmp(c.lutPath, a.lutPath) == 0 &&
                            c.overall == 60;
        CHECK(fileOk, "look: zapis i odczyt przez plik");
        DeleteFileW(path.c_str());
        CHECK(GetFileAttributesW((path + L".tmp").c_str()) == INVALID_FILE_ATTRIBUTES, "look: brak plikow .tmp");
        CHECK(!LoadLookFile(path + L".nie_ma", c), "look: brak pliku -> blad");

        std::string crlf;
        for (char ch : text) { if (ch == '\n') crlf += "\r\n"; else crlf += ch; }
        crlf = std::string("\xEF\xBB\xBF") + crlf;
        FilterParams d;
        CHECK(LookFromText(crlf.data(), crlf.size(), d) && d.overall == 60, "look: BOM i konce linii CRLF");
    }

    // --- Preset Look: odrzucanie zlych danych (parametry bez zmian)
    {
        const char* bad[] = {
            "", "garbage", "ZKLOOK 2\noverall 50\n", "ZKLOOK 1\n", "ZKLOOK 1\noverall 50\n",
            "ZKLOOK 1\ncurve 0 99 0 0 1000000 1000000\n", "ZKLOOK 1\ncurve 9 2 0 0 1000000 1000000\n",
            "ZKLOOK 1\ncurve 0 3 0 0 1000000 1000000\n", "ZKLOOK 1\nbw 1 2 3\n", "ZKLOOK 1\nbw a b c d e f g h i\n",
            "ZKLOOK 1\ntoning 1 2\n", "ZKLOOK 1\nduotone 1 2 3 4\n", "ZKLOOK 1\ncurve 0 2 0 0 1000000 abc\n",
        };
        bool allRejected = true, unchanged = true;
        for (const char* t : bad) {
            FilterParams p;
            p.overall = 77;
            if (LookFromText(t, std::strlen(t), p)) allRejected = false;
            if (p.overall != 77) unchanged = false;
        }
        const char withNul[] = "ZKLOOK 1\ntoning 1 2 3 4 5 6\n\0overall 5\n";
        FilterParams n;
        if (LookFromText(withNul, sizeof withNul, n)) allRejected = false;
        CHECK(allRejected, "look: uszkodzone pliki odrzucone (13 przypadkow + bajt zerowy)");
        CHECK(unchanged, "look: po bledzie parametry bez zmian");

        const std::string huge(2 << 20, 'a');
        FilterParams h;
        CHECK(!LookFromText(huge.data(), huge.size(), h), "look: plik powyzej 1 MB odrzucony");

        // Sciezki LUT z presetu: sieciowe (UNC), urzadzen i spoza .cube odrzucone; zwykla sciezka .cube przyjeta.
        const char* badPaths[] = {"\\\\serwer\\udzial\\x.cube", "//serwer/udzial/x.cube", "\\\\?\\C:\\x.cube",
                                  "\\\\.\\pipe\\x.cube", "C:\\Windows\\system32\\cmd.exe", "C:\\x.cube.txt", "x"};
        bool pathsRejected = true;
        for (const char* bp : badPaths) {
            std::string t = std::string("ZKLOOK 1\ntoning 1 2 3 4 5 6\nlutpath ") + bp + "\n";
            FilterParams p;
            if (LookFromText(t.data(), t.size(), p)) pathsRejected = false;
        }
        CHECK(pathsRejected, "look: sciezka LUT sieciowa (UNC), urzadzenia lub nie-.cube odrzucona");
        const std::string goodPath = "ZKLOOK 1\ntoning 1 2 3 4 5 6\nlutpath D:\\Moje LUT-y\\Film.CUBE\n";
        FilterParams gp;
        CHECK(LookFromText(goodPath.data(), goodPath.size(), gp) && wcscmp(gp.lutPath, L"D:\\Moje LUT-y\\Film.CUBE") == 0,
              "look: zwykla sciezka lokalna .cube przyjeta");

        // Nieznane klucze pomijane, wartosci spoza zakresu przyciete.
        const char* ok = "ZKLOOK 1\nfutureThing 1 2 3\ntoning 1 2 999 4 999 5000\noverall 999\nbw 1 9999 0 0 0 0 -9999 400 500\n";
        FilterParams p;
        const bool res = LookFromText(ok, std::strlen(ok), p);
        CHECK(res && p.overall == 100 && p.toning.hiSat == 100 && p.toning.balance == 100 && p.bw.mix[0] == 300 &&
                  p.bw.mix[5] == -200 && p.bw.tintHue == 360 && p.bw.tintSat == 100,
              "look: nieznane klucze pominiete, wartosci spoza zakresu przyciete");
    }

    // --- Eksport potoku do .cube
    {
        FilterParams id;  // domyslny wyglad = tozsamosc
        std::string t = ExportPipelineToCube(id, nullptr, 17, "Tozsamosc \"X\"");
        CubeLut lut;
        CubeResult r = ParseCubeText(t.data(), t.size(), lut);
        bool identity = r.error == CubeError::Ok && lut.size3d == 17 && lut.title == "Tozsamosc X";
        for (int bl = 0; identity && bl < 17; ++bl)
            for (int g = 0; g < 17; ++g)
                for (int rr = 0; rr < 17; ++rr) {
                    const float* v = &lut.data3d[(((size_t)bl * 17 + g) * 17 + rr) * 3];
                    identity = identity && Near(v[0], rr / 16.0f, 2e-6f) && Near(v[1], g / 16.0f, 2e-6f) &&
                               Near(v[2], bl / 16.0f, 2e-6f);
                }
        CHECK(identity, "eksport .cube: domyslny wyglad daje LUT tozsamosciowy, cudzyslow w tytule usuniety");

        // Gladki wyglad (krzywe + toning + duotone slabe + LUT): LUT 33^3 odtwarza potok z dokladnoscia ~0,02.
        FilterParams p;
        ApplyCurvePreset(CurvePreset::MediumS, p.curves);
        p.toning.on = 1; p.toning.hiSat = 30; p.toning.shSat = 30; p.toning.hiHue = 40; p.toning.shHue = 210;
        p.overall = 80;
        CubeLut base;
        std::string idText = IdentityCube(9);
        ParseCubeText(idText.data(), idText.size(), base);
        std::string exp = ExportPipelineToCube(p, &base, 33, "Look");
        CubeLut out;
        const bool parsed = ParseCubeText(exp.data(), exp.size(), out).error == CubeError::Ok && out.size3d == 33;
        CurveTables ct;
        PipelineOptions o = MakeOptions(p, ct, &base);
        std::mt19937 rng(5);
        float worst = 0;
        for (int i = 0; i < 400; ++i) {
            float in[3] = {(rng() % 1001) / 1000.0f, (rng() % 1001) / 1000.0f, (rng() % 1001) / 1000.0f}, a[3], b[3];
            ProcessPixel(o, in, a);
            SampleCube(out, in, b);
            for (int c = 0; c < 3; ++c) worst = std::max(worst, std::fabs(a[c] - b[c]));
        }
        char m[160];
        std::snprintf(m, sizeof m, "eksport .cube: LUT 33^3 odtwarza potok (max blad %.4f <= 0,02)", worst);
        CHECK(parsed && worst <= 0.02f, m);

        // Wartosci i tak zawsze w 0..1, rozmiar przyciety do 2..65.
        std::string tiny = ExportPipelineToCube(p, nullptr, 1, "x");
        std::string big = ExportPipelineToCube(p, nullptr, 500, "x");
        CubeLut a2, b2;
        CHECK(ParseCubeText(tiny.data(), tiny.size(), a2).error == CubeError::Ok && a2.size3d == 2 &&
                  ParseCubeText(big.data(), big.size(), b2).error == CubeError::Ok && b2.size3d == 65,
              "eksport .cube: rozmiar przyciety do 2..65");
    }

    // --- Parser: konce linii i '#'
    {
        std::string s = "# komentarz\rTITLE \"Look #3\"\rLUT_3D_SIZE 2\r";
        for (int b = 0; b < 2; ++b)
            for (int g = 0; g < 2; ++g)
                for (int r = 0; r < 2; ++r) s += std::to_string(r) + " " + std::to_string(g) + " " + std::to_string(b) + "\r";
        CubeLut l;
        CubeResult r = ParseCubeText(s.data(), s.size(), l);
        CHECK(r.error == CubeError::Ok && l.size3d == 2, "parser: konce linii samym CR (stary Mac)");
        CHECK(l.title == "Look #3", "parser: '#' w cudzyslowie TITLE nie jest komentarzem");
        std::string mixed = "LUT_3D_SIZE 2\r\n# x\n";
        for (int i = 0; i < 8; ++i) mixed += (i % 2 ? "1 0 0 # koniec\r\n" : "0 0 0\n");
        CHECK(ParseErr(mixed) == CubeError::Ok, "parser: mieszane \\r\\n, \\n i komentarz na koncu wiersza");
    }

    // --- Probkowanie 3D: wezly siatki odtwarzane dokladnie (obie metody), wynik w granicach rogow komorki
    {
        CubeLut l;
        l.size3d = 5;
        std::mt19937 rng(11);
        for (int i = 0; i < 5 * 5 * 5 * 3; ++i) l.data3d.push_back((rng() % 1001) / 1000.0f);
        bool nodes = true, hull = true;
        for (int tri = 0; tri <= 1; ++tri)
            for (int b = 0; b < 5; ++b)
                for (int g = 0; g < 5; ++g)
                    for (int r = 0; r < 5; ++r) {
                        float in[3] = {r / 4.0f, g / 4.0f, b / 4.0f}, o[3];
                        SampleCube(l, in, o, tri != 0);
                        const float* d = &l.data3d[(((size_t)b * 5 + g) * 5 + r) * 3];
                        for (int c = 0; c < 3; ++c) nodes = nodes && Near(o[c], d[c], 1e-5f);
                    }
        for (int i = 0; i < 2000; ++i) {
            float in[3] = {(rng() % 1001) / 1000.0f, (rng() % 1001) / 1000.0f, (rng() % 1001) / 1000.0f}, o[3];
            SampleCube(l, in, o, false);
            int i0[3];
            for (int c = 0; c < 3; ++c) i0[c] = std::min(3, (int)(in[c] * 4));
            for (int c = 0; c < 3; ++c) {
                float lo = 2, hi = -2;
                for (int k = 0; k < 8; ++k) {
                    const float v = l.data3d[(((size_t)(i0[2] + (k >> 2 & 1)) * 5 + (i0[1] + (k >> 1 & 1))) * 5 +
                                              (i0[0] + (k & 1))) * 3 + c];
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
                hull = hull && o[c] >= lo - 1e-5f && o[c] <= hi + 1e-5f;
            }
        }
        CHECK(nodes, "LUT 3D (losowy 5^3): wezly siatki odtworzone dokladnie (tetraedralnie i trojliniowo)");
        CHECK(hull, "LUT 3D (losowy 5^3): interpolacja tetraedralna w granicach rogow komorki");
    }

    // --- Zapis atomowy
    {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        const std::wstring path = std::wstring(tmp) + L"zk_atomic_" + std::to_wstring(GetCurrentProcessId()) + L".txt";
        const bool w1 = WriteFileAtomic(path, "pierwszy");
        const bool w2 = WriteFileAtomic(path, std::string(100000, 'z'));
        WIN32_FILE_ATTRIBUTE_DATA fa;
        const bool exists = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa) != 0;
        CHECK(w1 && w2 && exists && fa.nFileSizeLow == 100000, "zapis atomowy: nadpisanie pliku, pelna zawartosc");
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((path + L"*").c_str(), &fd);
        int files = 0;
        if (h != INVALID_HANDLE_VALUE) { do ++files; while (FindNextFileW(h, &fd)); FindClose(h); }
        DeleteFileW(path.c_str());
        CHECK(files == 1, "zapis atomowy: bez pozostawionych plikow tymczasowych");
        CHECK(!WriteFileAtomic(L"Z:\\nie\\ma\\takiego\\folderu\\plik.txt", "x"), "zapis atomowy: blad zwracany dla zlej sciezki");
    }
    // --- Klasyczny PIPL (dla Affinity i innych hostow, ktore nie czytaja JSON)
    {
        const std::vector<uint8_t> b = BuildClassicPipl();
        auto u32 = [&](size_t o) { return (uint32_t)b[o] | (uint32_t)b[o + 1] << 8 | (uint32_t)b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; };
        bool ok = b.size() > 10 && b[0] == 1;
        const uint32_t count = ok ? u32(6) : 0;
        size_t o = 10;
        std::vector<std::string> keys;
        for (uint32_t i = 0; ok && i < count; ++i) {
            if (o + 16 > b.size() || std::memcmp(&b[o], "MIB8", 4) != 0) { ok = false; break; }
            std::string key(4, ' ');
            for (int k = 0; k < 4; ++k) key[(size_t)k] = (char)b[o + 7 - (size_t)k];
            const uint32_t len = u32(o + 12);
            if (len % 4 != 0 || o + 16 + len > b.size() || u32(o + 8) != 0) { ok = false; break; }
            keys.push_back(key);
            o += 16 + len;
        }
        ok = ok && o == b.size();
        auto has = [&](const char* k) { return std::find(keys.begin(), keys.end(), k) != keys.end(); };
        CHECK(ok && count == 12 && has("kind") && has("name") && has("catg") && has("8664") && has("enbl") &&
                  has("mode") && has("fici"),
              "PIPL binarny: poprawny uklad (12 wlasciwosci, dlugosci wielokrotnosci 4, brak nadmiaru)");
        bool entry = false, kindOk = false;
        const std::string all(b.begin(), b.end());
        entry = all.find("PluginMain") != std::string::npos;
        kindOk = all.find("MFB8") != std::string::npos;
        CHECK(entry && kindOk, "PIPL binarny: punkt wejscia PluginMain i rodzaj filtra (8BFM)");
    }
}
