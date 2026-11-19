#include "core/pipl_binary.h"

#include <cstring>
#include <string>

namespace zk {

namespace {

struct Writer {
    std::vector<uint8_t> out;
    int count = 0;

    void U32(std::vector<uint8_t>& v, uint32_t x) {
        for (int i = 0; i < 4; ++i) v.push_back((uint8_t)(x >> (8 * i)));
    }
    static void Pad4(std::vector<uint8_t>& v) {
        while (v.size() % 4) v.push_back(0);
    }
    // Klucz zapisany odwrocony ("kind" -> "dnik"), jak w wersji little-endian.
    void Prop(const char* key, std::vector<uint8_t> data) {
        static const char kVendor[5] = "MIB8";
        for (int i = 0; i < 4; ++i) out.push_back((uint8_t)kVendor[i]);
        for (int i = 3; i >= 0; --i) out.push_back((uint8_t)key[i]);
        U32(out, 0);  // indeks
        U32(out, (uint32_t)(data.size() + ((4 - data.size() % 4) % 4)));
        Pad4(data);
        out.insert(out.end(), data.begin(), data.end());
        ++count;
    }
    std::vector<uint8_t> Pascal(const std::string& s) {
        std::vector<uint8_t> v;
        v.push_back((uint8_t)s.size());
        v.insert(v.end(), s.begin(), s.end());
        return v;
    }
    std::vector<uint8_t> CStr(const std::string& s) {
        std::vector<uint8_t> v(s.begin(), s.end());
        v.push_back(0);
        return v;
    }
    std::vector<uint8_t> Long(uint32_t x) {
        std::vector<uint8_t> v;
        U32(v, x);
        return v;
    }
};

}  // namespace

std::vector<uint8_t> BuildClassicPipl() {
    Writer w;
    // Rodzaj: filtr, OSType '8BFM' zapisany odwrocony (bajty "MFB8"), jak w innych wtyczkach.
    w.Prop("kind", {'M', 'F', 'B', '8'});

    w.Prop("name", w.Pascal("Kolor (krzywe, LUT, toning)..."));
    w.Prop("catg", w.Pascal("Zetmar"));
    w.Prop("vers", w.Long(0x00040000));  // wersja PIPL 4.0 (Version 4, SubVersion 0)
    {
        std::vector<uint8_t> c = w.Long(0x00010000);  // wersja komponentu 1.0.0
        const std::vector<uint8_t> n = w.CStr("Zetmar Kolor");
        c.insert(c.end(), n.begin(), n.end());
        w.Prop("cmpt", c);
    }
    w.Prop("8664", w.CStr("PluginMain"));  // punkt wejscia Win64
    {
        // Obslugiwane tryby (maska 16-bitowa): RGB (0x1000) i RGB 16 bit (0x0010); resztami steruje EnableInfo.
        std::vector<uint8_t> m = {0x10, 0x10, 0, 0};
        w.Prop("mode", m);
    }
    w.Prop("enbl", w.CStr("in (PSHOP_ImageMode, RGBMode, RGB48Mode) && PSHOP_ImageDepth != 32"));
    {
        // Najwiekszy rozmiar obrazu: 2 000 000 x 2 000 000 (0x1E8480).
        std::vector<uint8_t> v;
        w.U32(v, 2000000);
        w.U32(v, 2000000);
        w.Prop("ms23", v);
    }
    w.Prop("pmsa", w.Long(1));      // MonitorScalingAware
    w.Prop("flly", w.Long(0x80));   // FilterLayerSupport
    {
        // 7 przypadkow filtra (FilterCaseInfo): wejscie, wyjscie, flagi (bit 1 = WorksWithBlankData), 0.
        // Przypadek 3 (zaznaczenie plywajace) nieobslugiwany.
        std::vector<uint8_t> f;
        for (int i = 0; i < 7; ++i) {
            const bool cant = i == 2;
            f.push_back(cant ? 0 : 1);
            f.push_back(cant ? 0 : 1);
            f.push_back(2);
            f.push_back(0);
        }
        w.Prop("fici", f);
    }

    std::vector<uint8_t> res = {1, 0, 0, 0, 0, 0};
    w.U32(res, (uint32_t)w.count);
    res.insert(res.end(), w.out.begin(), w.out.end());
    return res;
}

}  // namespace zk
