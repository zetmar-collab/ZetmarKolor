// Warstwa hosta: odczyt informacji z FilterRecord, podglad, zapis kafelkami, raport diagnostyczny.
#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/bw_toning.h"
#include "core/curves.h"
#include "core/pipeline.h"

struct FilterRecord;

namespace zk {

// Parametry trzymane w uchwycie parametrow filtra (Ctrl+F powtarza bez okna).
struct FilterParams {
    uint32_t magic = 0x5A4B3144;  // 'ZK1D' (wersja 1.0 z duotonem)
    int32_t effect = 0;           // TestEffect (0 = brak)
    int32_t strength = 100;       // 0..100
    int32_t maskMode = 0;         // 0 = host (autoMask), 1 = wlasne (maskData)
    int32_t tileMode = 0;         // 0 = paski ~16 MB, 1 = kafelki 256x256, 2 = kafelki 1024x1024
    int32_t markTiles = 0;        // 1 = rysuj granice kafelkow (test wizualny)
    int32_t dither = 1;
    int32_t threads = 0;          // 0 = automatycznie
    int32_t lutStrength = 100;    // 0..100
    int32_t lutTrilinear = 0;     // 0 = tetraedralnie, 1 = trojliniowo (porownania)
    wchar_t lutPath[1024] = {0};  // pusty = bez LUT-a
    int32_t curvesOn = 1;
    CurveSet curves;
    BwParams bw;                  // czern-biel / sepia / monochromia
    ToningParams toning;
    DuotoneParams duotone;
    int32_t overall = 100;        // sila ogolna 0..100
};
void SetDefaults(FilterParams& p);
bool IsValid(const FilterParams& p);
void SanitizeParams(FilterParams& p);  // wartosci w poprawnych zakresach (po odczycie z uchwytu)
// Opcje potoku z parametrow - wspolne dla podgladu i zapisu (ct: bufor na tablice krzywych).
PipelineOptions MakeOptions(const FilterParams& p, CurveTables& ct, const CubeLut* lut);

struct IRect {
    int left = 0, top = 0, right = 0, bottom = 0;
    int w() const { return right - left; }
    int h() const { return bottom - top; }
};

struct HostInfo {
    int imageW = 0, imageH = 0;
    IRect filterRect;
    int depth = 8;
    int planes = 0;
    int imageMode = 0;
    int filterCase = 0;
    int transPlane = -1;
    bool rgbOk = false;
    bool haveMask = false;
    bool isFloating = false;
    bool autoMaskInitial = false;
    int layerOffsetX = 0, layerOffsetY = 0;
    bool layerOffsetKnown = false;
    HWND hwnd = nullptr;
};

HostInfo QueryHost(FilterRecord* fr);

// Pelny opis pol FilterRecord (wieloliniowy tekst do raportu i okna).
std::wstring DescribeHost(FilterRecord* fr, const HostInfo& hi);

struct PreviewImage {
    int w = 0, h = 0;
    std::vector<float> rgba;  // prosty RGBA 0..1
    std::vector<uint8_t> sel;  // zaznaczenie w podgladzie (255 = zaznaczone), puste gdy brak
};

// Metoda A: channel ports (targetCompositeChannels + targetTransparency) ze skalowaniem po stronie hosta.
// Metoda B: advanceState z inputRate (probkowanie calkowite). Obie sa probowane; wynik w `log`.
bool ReadPreview(FilterRecord* fr, const HostInfo& hi, int maxDim, PreviewImage& out, std::wstring& log);

enum class ApplyStatus { Ok, Cancelled, HostError };

struct ApplyStats {
    int tiles = 0;
    double ms = 0;
    int maskMin = 256, maskMax = -1;
    long long maskZero = 0, maskFull = 0, maskPartial = 0, maskSamples = 0;
    bool ownMaskActive = false;
    bool maskDataSeen = false;
    int autoMaskAfter = -1;
};

ApplyStatus Apply(FilterRecord* fr, const HostInfo& hi, const FilterParams& p, const CubeLut* lut, ApplyStats& st,
                  short& hostErr);

void DebugLog(const wchar_t* fmt, ...);
void AppendReport(const std::wstring& text);  // zawsze: %APPDATA%\ZetmarKolor\diagnostic_report.txt
std::string WideToUtf8(const std::wstring& w);

}  // namespace zk
