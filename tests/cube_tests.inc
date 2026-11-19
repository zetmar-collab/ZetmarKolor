// Testy parsera .cube i probkowania LUT-ow (dolaczane do core_tests.cpp).
#include <windows.h>

#include "core/cube_lut.h"

static std::string IdentityCube(int n, const char* eol = "\n", const char* header = "") {
    std::string s = std::string("# komentarz") + eol + "TITLE \"Identity\"" + eol + header + "LUT_3D_SIZE " +
                    std::to_string(n) + eol;
    char b[96];
    for (int bl = 0; bl < n; ++bl)
        for (int g = 0; g < n; ++g)
            for (int r = 0; r < n; ++r) {
                std::snprintf(b, sizeof b, "%.6f %.6f %.6f%s", r / (double)(n - 1), g / (double)(n - 1),
                              bl / (double)(n - 1), eol);
                s += b;
            }
    return s;
}

static CubeError ParseErr(const std::string& s) {
    CubeLut l;
    return ParseCubeText(s.data(), s.size(), l).error;
}

static int MaxDiff(const Img& a, const std::vector<uint8_t>& b) {
    int m = 0;
    if (a.bps == 1) {
        for (size_t i = 0; i < b.size(); ++i) m = std::max(m, std::abs((int)a.d[i] - (int)b[i]));
    } else {
        const uint16_t* x = (const uint16_t*)a.d.data();
        const uint16_t* y = (const uint16_t*)b.data();
        for (size_t i = 0; i < b.size() / 2; ++i) m = std::max(m, std::abs((int)x[i] - (int)y[i]));
    }
    return m;
}

static void RunCubeTests() {
    // --- LUT tozsamosciowy 33^3: wynik w granicach +-1 poziomu (8 i 16 bit, tetra i trilinear, z ditheringiem).
    CubeLut id;
    std::string idText = IdentityCube(33);
    CHECK(ParseCubeText(idText.data(), idText.size(), id).error == CubeError::Ok && id.size3d == 33 &&
              id.title == "Identity",
          "parser: tozsamosc 33^3 z TITLE i komentarzem");
    for (int bps = 1; bps <= 2; ++bps) {
        for (int tri = 0; tri <= 1; ++tri) {
            Img im = MakeImg(257, 129, 4, bps, 77 + bps);
            PipelineOptions o;
            o.effect = TestEffect::None;
            o.lut = &id;
            o.lutTrilinear = tri != 0;
            o.dither = true;
            auto out = Whole(o, im, 4);
            char m[128];
            int d = MaxDiff(im, out);
            std::snprintf(m, sizeof m, "LUT tozsamosciowy 33^3: max roznica %d <= 1 (%d bit, %s)", d, bps * 8,
                          tri ? "trojliniowo" : "tetraedralnie");
            CHECK(d <= 1, m);
        }
    }
    {
        // Tozsamosc bez ditheringu na 8 bit: dokladnie te same wartosci.
        Img im = MakeImg(200, 100, 3, 1, 5);
        PipelineOptions o;
        o.effect = TestEffect::None;
        o.lut = &id;
        o.dither = false;
        CHECK(MaxDiff(im, Whole(o, im, 2)) == 0, "LUT tozsamosciowy 33^3 bez ditheringu: 8 bit bez zmian");
    }

    // --- Kafelki i watki z LUT-em.
    {
        CubeLut inv;
        std::string t = "LUT_3D_SIZE 2\n1 1 1\n0 1 1\n1 0 1\n0 0 1\n1 1 0\n0 1 0\n1 0 0\n0 0 0\n";
        CHECK(ParseCubeText(t.data(), t.size(), inv).error == CubeError::Ok, "parser: LUT 2^3 (negatyw)");
        float in[3] = {0.2f, 0.7f, 0.45f}, out[3];
        SampleCube(inv, in, out);
        CHECK(std::fabs(out[0] - 0.8f) < 1e-5f && std::fabs(out[1] - 0.3f) < 1e-5f && std::fabs(out[2] - 0.55f) < 1e-5f,
              "probkowanie tetraedralne: negatyw 2^3 daje 1-x");
        SampleCube(inv, in, out, true);
        CHECK(std::fabs(out[0] - 0.8f) < 1e-5f && std::fabs(out[1] - 0.3f) < 1e-5f && std::fabs(out[2] - 0.55f) < 1e-5f,
              "probkowanie trojliniowe: negatyw 2^3 daje 1-x");
        Img im = MakeImg(301, 177, 4, 2, 9);
        PipelineOptions o;
        o.effect = TestEffect::SCurve;
        o.strength = 0.5f;
        o.lut = &inv;
        o.lutStrength = 0.6f;
        auto ref = Whole(o, im, 1);
        CHECK(Tiled(o, im, 37, 53, 3) == ref && Whole(o, im, 8) == ref, "LUT: wynik niezalezny od kafelkow i watkow");
    }

    // --- Poprawne warianty pliku.
    CHECK(ParseErr(IdentityCube(17, "\r\n")) == CubeError::Ok, "parser: konce linii CRLF");
    CHECK(ParseErr("\xEF\xBB\xBF" + IdentityCube(5)) == CubeError::Ok, "parser: BOM UTF-8");
    {
        CubeLut l;
        std::string t = IdentityCube(5, "\n", "DOMAIN_MIN 0 0 0\nDOMAIN_MAX 2 2 2\n");
        CHECK(ParseCubeText(t.data(), t.size(), l).error == CubeError::Ok && l.domainMax[1] == 2.0f,
              "parser: DOMAIN_MIN / DOMAIN_MAX");
        float in[3] = {1.0f, 0.5f, 2.0f}, out[3];
        SampleCube(l, in, out);
        CHECK(std::fabs(out[0] - 0.5f) < 1e-4f && std::fabs(out[1] - 0.25f) < 1e-4f && std::fabs(out[2] - 1.0f) < 1e-4f,
              "probkowanie: DOMAIN_MAX 2 skaluje wejscie");
    }
    {
        CubeLut l;
        std::string t = "TITLE \"1D\"\nLUT_1D_SIZE 3\n# dane\n0 0 0\n0.25 0.25 0.25\n1 1 1\n";
        CHECK(ParseCubeText(t.data(), t.size(), l).error == CubeError::Ok && l.size1d == 3 && l.size3d == 0,
              "parser: LUT 1D");
        float in[3] = {0.25f, 0.5f, 0.75f}, out[3];
        SampleCube(l, in, out);
        CHECK(std::fabs(out[0] - 0.125f) < 1e-5f && std::fabs(out[1] - 0.25f) < 1e-5f && std::fabs(out[2] - 0.625f) < 1e-5f,
              "probkowanie 1D liniowe");
    }
    CHECK(ParseErr("LUT_3D_SIZE 2\nLUT_IN_VIDEO_RANGE\n" + IdentityCube(2).substr(IdentityCube(2).find("0.0"))) ==
              CubeError::Ok,
          "parser: nieznane slowo kluczowe w naglowku jest pomijane");
    CHECK(ParseErr("LUT_3D_SIZE 2\n0 0 0\n1e0 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n") == CubeError::Ok,
          "parser: zapis wykladniczy");
    CHECK(ParseErr(IdentityCube(65)) == CubeError::Ok, "parser: rozmiar 65 (maksymalny)");

    // --- Uszkodzone pliki: czytelny blad, bez crasha.
    CHECK(ParseErr("") == CubeError::NoSize, "uszkodzony: pusty plik");
    CHECK(ParseErr("# tylko komentarz\n") == CubeError::NoSize, "uszkodzony: brak rozmiaru");
    CHECK(ParseErr("0 0 0\n1 1 1\n") == CubeError::NoSize, "uszkodzony: dane bez rozmiaru");
    CHECK(ParseErr("LUT_3D_SIZE 1\n0 0 0\n") == CubeError::BadSize, "uszkodzony: rozmiar 1");
    CHECK(ParseErr("LUT_3D_SIZE 66\n") == CubeError::BadSize, "uszkodzony: rozmiar 66");
    CHECK(ParseErr("LUT_3D_SIZE -5\n") == CubeError::BadSize, "uszkodzony: rozmiar ujemny");
    CHECK(ParseErr("LUT_3D_SIZE 99999999999999\n") == CubeError::BadSize ||
              ParseErr("LUT_3D_SIZE 99999999999999\n") == CubeError::BadKeyword,
          "uszkodzony: rozmiar ogromny");
    CHECK(ParseErr("LUT_3D_SIZE\n") == CubeError::BadKeyword, "uszkodzony: LUT_3D_SIZE bez liczby");
    CHECK(ParseErr("LUT_3D_SIZE abc\n") == CubeError::BadKeyword, "uszkodzony: LUT_3D_SIZE z tekstem");
    {
        std::string t = IdentityCube(3);
        CHECK(ParseErr(t.substr(0, t.rfind('\n', t.size() - 2) + 1)) == CubeError::WrongRowCount,
              "uszkodzony: za malo wierszy");
        CHECK(ParseErr(t + "0 0 0\n") == CubeError::WrongRowCount, "uszkodzony: za duzo wierszy");
        std::string nan = t;
        nan.replace(nan.rfind("1.000000"), 8, "nan");
        CHECK(ParseErr(nan) == CubeError::BadNumber, "uszkodzony: NaN");
        std::string inf = t;
        inf.replace(inf.rfind("1.000000"), 8, "1e999");
        CHECK(ParseErr(inf) == CubeError::BadNumber, "uszkodzony: nieskonczonosc");
        std::string two = t;
        two.replace(two.rfind(" 1.000000"), 9, "");
        CHECK(ParseErr(two) == CubeError::BadNumber, "uszkodzony: wiersz z 2 liczbami");
        std::string four = t + "";
        four.insert(four.rfind('\n'), " 0.5");
        CHECK(ParseErr(four) == CubeError::BadNumber, "uszkodzony: wiersz z 4 liczbami");
        std::string comma = t;
        comma.replace(comma.rfind("1.000000"), 8, "1,000000");
        CHECK(ParseErr(comma) == CubeError::BadNumber, "uszkodzony: przecinek dziesietny");
        CHECK(ParseErr(t + "LUT_3D_SIZE 3\n") == CubeError::BadKeyword, "uszkodzony: naglowek po danych");
        std::string bin = t;
        bin[bin.size() / 2] = '\0';
        CHECK(ParseErr(bin) == CubeError::Binary, "uszkodzony: bajt zerowy (dane binarne)");
        std::string junk = "\x89PNG\r\n\x1a\n" + t;
        CHECK(ParseErr(junk) == CubeError::Binary, "uszkodzony: plik PNG zamiast .cube");
        CHECK(ParseErr("%%% smieci %%%\n" + t) == CubeError::BadKeyword, "uszkodzony: smieci tekstowe w naglowku");
    }
    CHECK(ParseErr("DOMAIN_MIN 1 0 0\nDOMAIN_MAX 1 1 1\n" + IdentityCube(2)) == CubeError::BadDomain,
          "uszkodzony: DOMAIN_MIN == DOMAIN_MAX");
    CHECK(ParseErr("DOMAIN_MIN 0 0\n" + IdentityCube(2)) == CubeError::BadKeyword, "uszkodzony: DOMAIN_MIN z 2 liczbami");
    {
        std::string big((size_t)kMaxCubeFileBytes + 1, ' ');
        CHECK(ParseErr(big) == CubeError::TooLarge, "uszkodzony: plik powyzej 64 MB");
    }
    {
        CubeLut l;
        CHECK(LoadCubeFile(L"Z:\\nie\\istnieje.cube", l).error == CubeError::OpenFailed, "uszkodzony: brak pliku");
    }
    // Wszystkie mozliwe przyciecia malego pliku: zaden nie moze wywolac crasha.
    {
        std::string t = IdentityCube(2);
        // Obciecie w srodku ostatniej liczby daje poprawny plik ("1.000000" -> "1.0"); wczesniej brakuje danych.
        const size_t lastSpace = t.rfind(' ');
        bool ok = true;
        for (size_t n = 0; n < t.size(); ++n) {
            CubeLut l;
            CubeResult r = ParseCubeText(t.data(), n, l);
            if (r.error == CubeError::Ok && n <= lastSpace + 1) ok = false;
        }
        CHECK(ok, "uszkodzony: obciete pliki (kazda dlugosc) bez crasha i bez falszywego sukcesu");
    }
    {
        // Losowe smieci.
        std::mt19937 rng(42);
        bool ok = true;
        for (int k = 0; k < 200; ++k) {
            std::string s(rng() % 4000, '\0');
            for (auto& c : s) c = (char)(rng() & 0xFF);
            CubeLut l;
            if (ParseCubeText(s.data(), s.size(), l).error == CubeError::Ok) ok = false;
        }
        CHECK(ok, "uszkodzony: 200 losowych plikow binarnych odrzuconych");
    }

    // --- Pliki z folderu LUT-ow (jesli jest obok repozytorium).
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(L"..\\Photoshop\\LUTS\\*.cube", &fd);
    if (f == INVALID_HANDLE_VALUE) f = FindFirstFileW(L"..\\..\\Photoshop\\LUTS\\*.cube", &fd);
    if (f != INVALID_HANDLE_VALUE) {
        std::wstring dir = GetFileAttributesW(L"..\\Photoshop\\LUTS") != INVALID_FILE_ATTRIBUTES ? L"..\\Photoshop\\LUTS\\"
                                                                                                 : L"..\\..\\Photoshop\\LUTS\\";
        int n = 0, bad = 0;
        do {
            CubeLut l;
            CubeResult r = LoadCubeFile(dir + fd.cFileName, l);
            ++n;
            if (r.error != CubeError::Ok) {
                ++bad;
                std::printf("      %ls: blad %d wiersz %d\n", fd.cFileName, (int)r.error, r.line);
            }
        } while (FindNextFileW(f, &fd));
        FindClose(f);
        char m[96];
        std::snprintf(m, sizeof m, "folder Photoshop\\LUTS: wczytano %d/%d plikow", n - bad, n);
        CHECK(bad == 0 && n > 0, m);
    } else {
        std::printf("pomin: brak folderu Photoshop\\LUTS\n");
    }
}
