// Parametry filtra: domyslne wartosci, walidacja i opcje potoku. Bez zaleznosci od SDK (czesc zkcore, testowane).
#include <algorithm>

#include "core/settings.h"
#include "plugin/host.h"

namespace zk {

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

void SetDefaults(FilterParams& p) { p = FilterParams(); }

void SanitizeParams(FilterParams& p) {
    SanitizeBw(p.bw);
    SanitizeToning(p.toning);
    SanitizeDuotone(p.duotone);
    p.overall = std::min(100, std::max(0, (int)p.overall));
    p.strength = std::min(100, std::max(0, (int)p.strength));
    p.lutStrength = std::min(100, std::max(0, (int)p.lutStrength));
    p.lutTrilinear = p.lutTrilinear ? 1 : 0;
    p.threads = std::min(64, std::max(0, (int)p.threads));
    for (auto& c : p.curves.c) SanitizeCurve(c);
    p.curves.lumaOnly = p.curves.lumaOnly ? 1 : 0;
    p.curvesOn = p.curvesOn ? 1 : 0;
    p.lutPath[1023] = 0;  // sciezka zawsze zakonczona zerem
    // Opcje testowe (zakladka Test) dzialaja tylko w trybie diagnostycznym; inaczej ukryte ustawienie
    // z poprzedniego uruchomienia zmienialoby obraz bez sladu w oknie.
    if (!DebugEnabled()) {
        p.effect = 0;
        p.strength = 100;
        p.maskMode = 0;
        p.tileMode = 0;
        p.markTiles = 0;
        p.dither = 1;
    }
}

bool IsValid(const FilterParams& p) {
    return p.magic == FilterParams().magic && p.effect >= 0 && p.effect < (int)TestEffect::Count &&
           p.strength >= 0 && p.strength <= 100 && p.maskMode >= 0 && p.maskMode <= 1 && p.tileMode >= 0 &&
           p.tileMode <= 2 && p.lutStrength >= 0 && p.lutStrength <= 100 && p.threads >= 0 && p.threads <= 64 &&
           std::find(p.lutPath, p.lutPath + 1024, wchar_t(0)) != p.lutPath + 1024;  // sciezka zakonczona zerem
}

PipelineOptions MakeOptions(const FilterParams& p, CurveTables& ct, const CubeLut* lut) {
    PipelineOptions o;
    o.effect = (TestEffect)p.effect;
    o.strength = p.strength / 100.0f;
    o.dither = p.dither != 0;
    ct = CurveTables();
    if (p.curvesOn) BuildCurveTables(p.curves, ct);
    o.curves = ct.active ? &ct : nullptr;
    o.bw = BwActive(p.bw) ? &p.bw : nullptr;
    o.duotone = DuotoneActive(p.duotone) ? &p.duotone : nullptr;
    o.duotoneMix = p.bw.mix;
    o.lut = lut;
    o.lutStrength = p.lutStrength / 100.0f;
    o.lutTrilinear = p.lutTrilinear != 0;
    o.toning = ToningActive(p.toning) ? &p.toning : nullptr;
    o.overall = p.overall / 100.0f;
    return o;
}

}  // namespace zk
