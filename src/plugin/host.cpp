#include <windows.h>

#include "PIDefines.h"
#include "PIFilter.h"
#include "PIProperties.h"
#include "PITypes.h"

#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "core/settings.h"
#include "plugin/host.h"

namespace zk {

// ---------------------------------------------------------------------------------------------
// Narzedzia

namespace {

void AppendFile(const std::wstring& path, const std::string& u) {
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(h, u.data(), (DWORD)u.size(), &w, nullptr);
    CloseHandle(h);
}

double NowMs() {
    static LARGE_INTEGER f = [] { LARGE_INTEGER q; QueryPerformanceFrequency(&q); return q; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

}  // namespace

void DebugLog(const wchar_t* fmt, ...) {
    std::wstring dir = DataDir();
    if (dir.empty()) return;
    if (GetFileAttributesW((dir + L"\\debug.flag").c_str()) == INVALID_FILE_ATTRIBUTES) return;
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    AppendFile(dir + L"\\log.txt", WideToUtf8(buf) + "\r\n");
}

void AppendReport(const std::wstring& text) {
    if (!DebugEnabled()) return;  // raport tylko w trybie diagnostycznym
    std::wstring dir = DataDir();
    if (dir.empty()) return;
    std::wstring t = text;
    // Normalizacja koncow linii do CRLF (Notatnik).
    std::wstring o;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == L'\n' && (i == 0 || t[i - 1] != L'\r')) o += L'\r';
        o += t[i];
    }
    AppendFile(dir + L"\\diagnostic_report.txt", WideToUtf8(o) + "\r\n");
}

// ---------------------------------------------------------------------------------------------
// Prostokaty filtra (32-bitowe, gdy host je udostepnia)

namespace {

bool UsesBig(const FilterRecord* fr) {
    return fr->bigDocumentData != nullptr && fr->bigDocumentData->PluginUsing32BitCoordinates;
}

void SetInOut(FilterRecord* fr, const IRect& in, const IRect& out) {
    if (UsesBig(fr)) {
        VRect a; a.left = in.left; a.top = in.top; a.right = in.right; a.bottom = in.bottom;
        VRect b; b.left = out.left; b.top = out.top; b.right = out.right; b.bottom = out.bottom;
        fr->bigDocumentData->inRect32 = a;
        fr->bigDocumentData->outRect32 = b;
    } else {
        Rect a; a.left = (int16)in.left; a.top = (int16)in.top; a.right = (int16)in.right; a.bottom = (int16)in.bottom;
        Rect b; b.left = (int16)out.left; b.top = (int16)out.top; b.right = (int16)out.right; b.bottom = (int16)out.bottom;
        fr->inRect = a;
        fr->outRect = b;
    }
}

void SetMask(FilterRecord* fr, const IRect& m) {
    if (UsesBig(fr)) {
        VRect a; a.left = m.left; a.top = m.top; a.right = m.right; a.bottom = m.bottom;
        fr->bigDocumentData->maskRect32 = a;
    } else {
        Rect a; a.left = (int16)m.left; a.top = (int16)m.top; a.right = (int16)m.right; a.bottom = (int16)m.bottom;
        fr->maskRect = a;
    }
}

void ZeroRects(FilterRecord* fr) {
    IRect z;
    SetInOut(fr, z, z);
    SetMask(fr, z);
}

const wchar_t* FilterCaseName(int fc) {
    switch (fc) {
        case 0: return L"0 (host nie ustawil)";
        case filterCaseFlatImageNoSelection: return L"1 FlatImageNoSelection";
        case filterCaseFlatImageWithSelection: return L"2 FlatImageWithSelection";
        case filterCaseFloatingSelection: return L"3 FloatingSelection";
        case filterCaseEditableTransparencyNoSelection: return L"4 EditableTransparencyNoSelection";
        case filterCaseEditableTransparencyWithSelection: return L"5 EditableTransparencyWithSelection";
        case filterCaseProtectedTransparencyNoSelection: return L"6 ProtectedTransparencyNoSelection";
        case filterCaseProtectedTransparencyWithSelection: return L"7 ProtectedTransparencyWithSelection";
        default: return L"?";
    }
}

std::wstring Sig(uint32_t s) {
    wchar_t b[5];
    for (int i = 0; i < 4; ++i) {
        wchar_t c = (wchar_t)((s >> (24 - 8 * i)) & 0xFF);
        b[i] = (c >= 32 && c < 127) ? c : L'?';
    }
    b[4] = 0;
    return b;
}

void Line(std::wstring& s, const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    s += buf;
    s += L"\r\n";
}

void DescribeChannels(std::wstring& s, const wchar_t* name, const ReadChannelDesc* c) {
    if (!c) { Line(s, L"  %s: brak", name); return; }
    int n = 0;
    for (const ReadChannelDesc* p = c; p && n < 16; p = p->next) {
        Line(s, L"  %s[%d]: bounds %d,%d,%d,%d depth %d tile %dx%d type %d name \"%hs\" ver %d..%d", name, n,
             p->bounds.left, p->bounds.top, p->bounds.right, p->bounds.bottom, p->depth, p->tileSize.h,
             p->tileSize.v, p->channelType, p->name ? p->name : "", p->minVersion, p->maxVersion);
        ++n;
    }
}

}  // namespace

HostInfo QueryHost(FilterRecord* fr) {
    HostInfo hi;
    if (UsesBig(fr)) {
        hi.imageW = fr->bigDocumentData->imageSize32.h;
        hi.imageH = fr->bigDocumentData->imageSize32.v;
        const VRect& r = fr->bigDocumentData->filterRect32;
        hi.filterRect = {r.left, r.top, r.right, r.bottom};
    } else {
        hi.imageW = fr->imageSize.h;
        hi.imageH = fr->imageSize.v;
        hi.filterRect = {fr->filterRect.left, fr->filterRect.top, fr->filterRect.right, fr->filterRect.bottom};
    }
    hi.depth = fr->depth > 0 ? fr->depth : 8;
    hi.planes = fr->planes;
    hi.imageMode = fr->imageMode;
    hi.filterCase = fr->filterCase;
    hi.rgbOk = (hi.imageMode == plugInModeRGBColor || hi.imageMode == plugInModeRGB48) &&
               (hi.depth == 8 || hi.depth == 16) && hi.planes >= 3;
    hi.haveMask = fr->haveMask != 0;
    hi.isFloating = fr->isFloating != 0;
    hi.autoMaskInitial = fr->autoMask != 0;

    const int fc = fr->filterCase;
    if (fc == filterCaseEditableTransparencyNoSelection || fc == filterCaseEditableTransparencyWithSelection) {
        // Uklad RGBA: przezroczystosc jest czwarta plaszczyzna (zaraz po 3 kanalach warstwy).
        if (hi.planes >= 4 && (fr->inTransparencyMask > 0 || fr->inLayerPlanes == 3)) hi.transPlane = 3;
    }
    if (fr->platformData) hi.hwnd = (HWND)((PlatformData*)fr->platformData)->hwnd;
    if (fr->documentInfo && fr->documentInfo->targetCompositeChannels) {
        hi.layerOffsetX = fr->documentInfo->targetCompositeChannels->bounds.left;
        hi.layerOffsetY = fr->documentInfo->targetCompositeChannels->bounds.top;
        hi.layerOffsetKnown = true;
    }
    return hi;
}

std::wstring DescribeHost(FilterRecord* fr, const HostInfo& hi) {
    std::wstring s;
    SYSTEMTIME t;
    GetLocalTime(&t);
    Line(s, L"=== ZetmarKolor raport, %04d-%02d-%02d %02d:%02d:%02d ===", t.wYear, t.wMonth, t.wDay, t.wHour,
         t.wMinute, t.wSecond);
    wchar_t exe[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    Line(s, L"host exe: %s", exe);
    Line(s, L"hostSig: '%s'", Sig((uint32_t)fr->hostSig).c_str());
    if (fr->propertyProcs && fr->propertyProcs->getPropertyProc) {
        intptr_t simple = 0;
        Handle complex = nullptr;
        OSErr e = fr->propertyProcs->getPropertyProc(kPhotoshopSignature, propVersion, 0, &simple, &complex);
        Line(s, L"propVersion: err %d value 0x%IX (major %d minor %d fix %d)", (int)e, simple,
             (int)((simple >> 16) & 0xFFFF), (int)((simple >> 8) & 0xFF), (int)(simple & 0xFF));
        if (complex && fr->handleProcs) fr->handleProcs->disposeProc(complex);
    } else {
        Line(s, L"propertyProcs: brak");
    }
    Line(s, L"imageMode %d (RGB=%d RGB48=%d), depth %d, planes %d, rgbOk %d", hi.imageMode, plugInModeRGBColor,
         plugInModeRGB48, hi.depth, hi.planes, (int)hi.rgbOk);
    Line(s, L"bigDocumentData: %s", fr->bigDocumentData ? L"jest" : L"BRAK");
    Line(s, L"imageSize: %d x %d   filterRect: %d,%d,%d,%d", hi.imageW, hi.imageH, hi.filterRect.left,
         hi.filterRect.top, hi.filterRect.right, hi.filterRect.bottom);
    Line(s, L"filterCase: %s", FilterCaseName(hi.filterCase));
    Line(s, L"haveMask %d, isFloating %d, autoMask (start) %d", (int)hi.haveMask, (int)hi.isFloating,
         (int)hi.autoMaskInitial);
    Line(s, L"inLayerPlanes %d, inTransparencyMask %d, inLayerMasks %d, inInvertedLayerMasks %d, inNonLayerPlanes %d",
         fr->inLayerPlanes, fr->inTransparencyMask, fr->inLayerMasks, fr->inInvertedLayerMasks, fr->inNonLayerPlanes);
    Line(s, L"outLayerPlanes %d, outTransparencyMask %d, outLayerMasks %d, outInvertedLayerMasks %d, outNonLayerPlanes %d",
         fr->outLayerPlanes, fr->outTransparencyMask, fr->outLayerMasks, fr->outInvertedLayerMasks,
         fr->outNonLayerPlanes);
    Line(s, L"transPlane (wyliczony): %d", hi.transPlane);
    Line(s, L"samplingSupport %d, supportsDummyChannels %d, supportsAlternateLayouts %d, supportsAbsolute %d",
         (int)fr->samplingSupport, (int)fr->supportsDummyChannels, (int)fr->supportsAlternateLayouts,
         (int)fr->supportsAbsolute);
    Line(s, L"maxSpace %d, maxSpace64 %lld", fr->maxSpace, (long long)fr->maxSpace64);
    Line(s, L"advanceState %s, progressProc %s, abortProc %s, displayPixels %s, handleProcs %s",
         fr->advanceState ? L"jest" : L"BRAK", fr->progressProc ? L"jest" : L"BRAK", fr->abortProc ? L"jest" : L"BRAK",
         fr->displayPixels ? L"jest" : L"BRAK", fr->handleProcs ? L"jest" : L"BRAK");
    Line(s, L"channelPortProcs %s, readPixelsProc %s", fr->channelPortProcs ? L"jest" : L"BRAK",
         fr->channelPortProcs && fr->channelPortProcs->readPixelsProc ? L"jest" : L"BRAK");
    Line(s, L"platformData hwnd %p", (void*)hi.hwnd);
    ReadImageDocumentDesc* d = fr->documentInfo;
    if (!d) {
        Line(s, L"documentInfo: BRAK");
    } else {
        Line(s, L"documentInfo: ver %d..%d mode %d depth %d bounds %d,%d,%d,%d ICC %s (%d B)", d->minVersion,
             d->maxVersion, d->imageMode, d->depth, d->bounds.left, d->bounds.top, d->bounds.right, d->bounds.bottom,
             d->iCCprofileData ? L"jest" : L"brak", (int)d->iCCprofileSize);
        DescribeChannels(s, L"targetComposite", d->targetCompositeChannels);
        DescribeChannels(s, L"targetTransparency", d->targetTransparency);
        DescribeChannels(s, L"targetLayerMask", d->targetLayerMask);
        DescribeChannels(s, L"mergedComposite", d->mergedCompositeChannels);
        DescribeChannels(s, L"selection", d->selection);
    }
    Line(s, L"layerOffset (z targetComposite.bounds): %d,%d known %d", hi.layerOffsetX, hi.layerOffsetY,
         (int)hi.layerOffsetKnown);
    return s;
}

// ---------------------------------------------------------------------------------------------
// Podglad

namespace {

// Odczytuje jeden kanal przez channel port z przeskalowaniem do pw x ph. Wynik 0..1.
bool ReadPort(FilterRecord* fr, const ReadChannelDesc* c, const IRect& src, int pw, int ph, std::vector<float>& out,
              std::wstring& log) {
    if (!c || !fr->channelPortProcs || !fr->channelPortProcs->readPixelsProc) return false;
    const int depth = c->depth == 16 ? 16 : 8;
    std::vector<uint8_t> buf((size_t)pw * ph * (depth / 8), 0);
    PSScaling sc;
    sc.sourceRect.left = src.left; sc.sourceRect.top = src.top;
    sc.sourceRect.right = src.right; sc.sourceRect.bottom = src.bottom;
    sc.destinationRect.left = 0; sc.destinationRect.top = 0;
    sc.destinationRect.right = pw; sc.destinationRect.bottom = ph;
    VRect wr = sc.destinationRect, wrote = {0, 0, 0, 0};
    PixelMemoryDesc d;
    d.data = buf.data();
    d.rowBits = pw * depth;
    d.colBits = depth;
    d.bitOffset = 0;
    d.depth = depth;
    OSErr e = fr->channelPortProcs->readPixelsProc(c->port, &sc, &wr, &d, &wrote);
    if (e != noErr) {
        log += L"    readPixelsProc err " + std::to_wstring(e) + L"\r\n";
        return false;
    }
    out.resize((size_t)pw * ph);
    const float maxv = depth == 16 ? 32768.0f : 255.0f;
    for (size_t i = 0; i < out.size(); ++i) {
        float v = depth == 16 ? ((const uint16_t*)buf.data())[i] / maxv : buf[i] / maxv;
        out[i] = std::min(std::max(v, 0.0f), 1.0f);
    }
    return true;
}

bool ReadViaPorts(FilterRecord* fr, const HostInfo& hi, int pw, int ph, PreviewImage& out, std::wstring& log) {
    ReadImageDocumentDesc* doc = fr->documentInfo;
    if (!doc || !doc->targetCompositeChannels) { log += L"    brak documentInfo/targetCompositeChannels\r\n"; return false; }
    const ReadChannelDesc* ch[3];
    const ReadChannelDesc* c = doc->targetCompositeChannels;
    for (int i = 0; i < 3; ++i) {
        if (!c) { log += L"    mniej niz 3 kanaly\r\n"; return false; }
        ch[i] = c;
        c = c->next;
    }
    IRect src{hi.layerOffsetX, hi.layerOffsetY, hi.layerOffsetX + hi.imageW, hi.layerOffsetY + hi.imageH};
    std::vector<float> p[4];
    for (int i = 0; i < 3; ++i)
        if (!ReadPort(fr, ch[i], src, pw, ph, p[i], log)) return false;
    bool alpha = doc->targetTransparency && ReadPort(fr, doc->targetTransparency, src, pw, ph, p[3], log);
    out.w = pw; out.h = ph;
    out.rgba.resize((size_t)pw * ph * 4);
    for (size_t i = 0; i < (size_t)pw * ph; ++i) {
        out.rgba[i * 4 + 0] = p[0][i];
        out.rgba[i * 4 + 1] = p[1][i];
        out.rgba[i * 4 + 2] = p[2][i];
        out.rgba[i * 4 + 3] = alpha ? p[3][i] : 1.0f;
    }
    log += alpha ? L"    alfa z targetTransparency\r\n" : L"    bez kanalu alfa\r\n";
    return true;
}

bool ReadViaRate(FilterRecord* fr, const HostInfo& hi, int rate, PreviewImage& out, std::wstring& log) {
    if (fr->samplingSupport == 0) { log += L"    samplingSupport = 0\r\n"; return false; }
    int pw = (hi.imageW + rate - 1) / rate, ph = (hi.imageH + rate - 1) / rate;
    fr->inputRate = (Fixed)rate << 16;
    fr->maskRate = (Fixed)rate << 16;
    fr->inputPadding = plugInWantsEdgeReplication;  // probki poza obrazem (zaokraglenie w gore)
    IRect r{0, 0, pw, ph}, z;
    SetInOut(fr, r, z);
    SetMask(fr, z);
    const int use = hi.transPlane >= 0 ? 4 : 3;
    fr->inLoPlane = 0;
    fr->inHiPlane = (int16)(use - 1);
    fr->outLoPlane = fr->outHiPlane = 0;
    OSErr e = fr->advanceState();
    bool ok = e == noErr && fr->inData;
    if (!ok) log += L"    advanceState err " + std::to_wstring(e) + L"\r\n";
    if (ok) {
        const int bps = hi.depth == 16 ? 2 : 1;
        const float maxv = hi.depth == 16 ? 32768.0f : 255.0f;
        out.w = pw; out.h = ph;
        out.rgba.assign((size_t)pw * ph * 4, 0.0f);
        for (int y = 0; y < ph; ++y) {
            const uint8_t* row = (const uint8_t*)fr->inData + (ptrdiff_t)y * fr->inRowBytes;
            for (int x = 0; x < pw; ++x) {
                const uint8_t* px = row + (size_t)x * use * bps;
                float* o = &out.rgba[((size_t)y * pw + x) * 4];
                o[3] = 1.0f;
                for (int c = 0; c < use; ++c) {
                    float f = bps == 2 ? ((const uint16_t*)px)[c] / maxv : px[c] / maxv;
                    o[c] = std::min(std::max(f, 0.0f), 1.0f);
                }
            }
        }
    }
    fr->inputRate = (Fixed)1 << 16;
    fr->maskRate = (Fixed)1 << 16;
    ZeroRects(fr);
    return ok;
}

// Lapiemy tylko bledy dostepu do pamieci z kodu hosta; wyjatki C++ (np. brak pamieci) ida dalej do PluginMain.
int SehFilter(unsigned long code) {
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER
                                                                                  : EXCEPTION_CONTINUE_SEARCH;
}

// __try nie moze stac w funkcji z obiektami C++ do zniszczenia, stad osobne opakowania.
bool SehReadViaPorts(FilterRecord* fr, const HostInfo& hi, int pw, int ph, PreviewImage& out, std::wstring& log) {
    __try {
        return ReadViaPorts(fr, hi, pw, ph, out, log);
    } __except (SehFilter(GetExceptionCode())) {
        return false;
    }
}

bool SehReadPort(FilterRecord* fr, const ReadChannelDesc* c, const IRect& src, int pw, int ph, std::vector<float>& out,
                 std::wstring& log) {
    __try {
        return ReadPort(fr, c, src, pw, ph, out, log);
    } __except (SehFilter(GetExceptionCode())) {
        return false;
    }
}

}  // namespace

bool ReadPreview(FilterRecord* fr, const HostInfo& hi, int maxDim, PreviewImage& out, std::wstring& log) {
    if (hi.imageW <= 0 || hi.imageH <= 0) return false;
    const int longest = std::max(hi.imageW, hi.imageH);
    const double s = std::min(1.0, (double)maxDim / longest);
    const int pw = std::max(1, (int)std::lround(hi.imageW * s)), ph = std::max(1, (int)std::lround(hi.imageH * s));

    // Metoda A (channel ports) jest szybsza i daje tez zaznaczenie. Metoda B (inputRate) jest zapasowa;
    // obie sa porownywane tylko w trybie diagnostycznym, bo drugi odczyt wydluza otwarcie okna.
    const bool debug = DebugEnabled();
    PreviewImage a, b;
    double t0 = NowMs();
    std::wstring logA;
    bool okA = SehReadViaPorts(fr, hi, pw, ph, a, logA);
    double t1 = NowMs();
    const int rate = std::max(1, (longest + maxDim - 1) / maxDim);
    std::wstring logB;
    bool okB = false;
    if (!okA || debug) okB = ReadViaRate(fr, hi, rate, b, logB);
    double t2 = NowMs();
    wchar_t buf[256];
    swprintf_s(buf, L"Podglad A (channel ports, %dx%d): %s, %.1f ms\r\n", pw, ph, okA ? L"OK" : L"NIE", t1 - t0);
    log += buf;
    log += logA;
    if (!okA || debug) {
        swprintf_s(buf, L"Podglad B (inputRate %d, %dx%d): %s, %.1f ms\r\n", rate, b.w, b.h, okB ? L"OK" : L"NIE", t2 - t1);
        log += buf;
        log += logB;
    }
    if (okA && okB) {
        // Porownanie metod (rozne rozmiary: porownujemy srodkowy piksel).
        const float* ca = &a.rgba[(((size_t)a.h / 2) * a.w + a.w / 2) * 4];
        const float* cb = &b.rgba[(((size_t)b.h / 2) * b.w + b.w / 2) * 4];
        swprintf_s(buf, L"  srodek A: %.3f %.3f %.3f a%.3f | B: %.3f %.3f %.3f a%.3f\r\n", ca[0], ca[1], ca[2], ca[3],
                   cb[0], cb[1], cb[2], cb[3]);
        log += buf;
    }
    if (okA) out = std::move(a);
    else if (okB) out = std::move(b);
    else return false;

    // Zaznaczenie do podgladu (tylko informacyjnie) i jego statystyka.
    if (hi.haveMask && fr->documentInfo && fr->documentInfo->selection) {
        std::vector<float> sel;
        std::wstring l;
        IRect src{hi.layerOffsetX, hi.layerOffsetY, hi.layerOffsetX + hi.imageW, hi.layerOffsetY + hi.imageH};
        bool okS = SehReadPort(fr, fr->documentInfo->selection, src, out.w, out.h, sel, l);
        if (okS) {
            out.sel.resize(sel.size());
            for (size_t i = 0; i < sel.size(); ++i) out.sel[i] = (uint8_t)std::lround(sel[i] * 255.0f);
            int cx = (hi.filterRect.left + hi.filterRect.right) / 2 * out.w / std::max(1, hi.imageW);
            int cy = (hi.filterRect.top + hi.filterRect.bottom) / 2 * out.h / std::max(1, hi.imageH);
            cx = std::min(std::max(cx, 0), out.w - 1);
            cy = std::min(std::max(cy, 0), out.h - 1);
            swprintf_s(buf, L"Zaznaczenie (documentInfo->selection): OK, srodek filterRect = %d, rog (0,0) = %d\r\n",
                       out.sel[(size_t)cy * out.w + cx], out.sel[0]);
            log += buf;
        } else {
            log += L"Zaznaczenie (documentInfo->selection): NIE\r\n" + l;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Zapis

ApplyStatus Apply(FilterRecord* fr, const HostInfo& hi, const FilterParams& p, const CubeLut* lut, ApplyStats& st,
                  short& hostErr) {
    hostErr = 0;
    st = ApplyStats();
    const double t0 = NowMs();
    const IRect fr0 = hi.filterRect;
    if (fr0.w() <= 0 || fr0.h() <= 0) return ApplyStatus::Ok;

    CurveTables ct;
    PipelineOptions o = MakeOptions(p, ct, lut);

    const int planes = hi.transPlane >= 0 ? 4 : 3;
    const int bps = hi.depth == 16 ? 2 : 1;

    fr->inputRate = (Fixed)1 << 16;
    fr->maskRate = (Fixed)1 << 16;
    fr->inLoPlane = fr->outLoPlane = 0;
    fr->inHiPlane = fr->outHiPlane = (int16)(planes - 1);

    // Tryb wlasnego maskowania: wylaczamy autoMask (dozwolone tylko, gdy haveMask i nie isFloating).
    const bool ownMask = p.maskMode == 1 && hi.haveMask && !hi.isFloating;
    if (ownMask) fr->autoMask = FALSE;
    st.ownMaskActive = ownMask;
    st.autoMaskAfter = fr->autoMask ? 1 : 0;

    int tw, th;
    if (p.tileMode == 1) { tw = th = 256; }
    else if (p.tileMode == 2) { tw = th = 1024; }
    else {
        tw = fr0.w();
        const long long rowBytes = (long long)tw * planes * bps;
        th = (int)std::max<long long>(1, (16LL << 20) / std::max<long long>(rowBytes, 1));
    }
    const int tilesX = (fr0.w() + tw - 1) / tw, tilesY = (fr0.h() + th - 1) / th;
    const int total = tilesX * tilesY;
    const int64_t offX = hi.layerOffsetKnown ? hi.layerOffsetX : 0, offY = hi.layerOffsetKnown ? hi.layerOffsetY : 0;
    const float maxv = bps == 2 ? 32768.0f : 255.0f;
    const bool collectMaskStats = DebugEnabled();  // statystyka maski tylko do raportu diagnostycznego

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            IRect r{fr0.left + tx * tw, fr0.top + ty * th, std::min(fr0.left + (tx + 1) * tw, fr0.right),
                    std::min(fr0.top + (ty + 1) * th, fr0.bottom)};
            SetInOut(fr, r, r);
            if (hi.haveMask) SetMask(fr, r);
            OSErr e = fr->advanceState();
            if (e != noErr) { hostErr = (short)e; ZeroRects(fr); return ApplyStatus::HostError; }
            if (!fr->inData || !fr->outData) { hostErr = filterBadParameters; ZeroRects(fr); return ApplyStatus::HostError; }

            const uint8_t* mask = nullptr;
            if (fr->maskData) {
                st.maskDataSeen = true;
                const uint8_t* m = (const uint8_t*)fr->maskData;
                for (int y = 0; collectMaskStats && y < r.h(); y += 4) {
                    for (int x = 0; x < r.w(); x += 4) {
                        int v = m[(ptrdiff_t)y * fr->maskRowBytes + x];
                        st.maskMin = std::min(st.maskMin, v);
                        st.maskMax = std::max(st.maskMax, v);
                        if (v == 0) ++st.maskZero;
                        else if (v == 255) ++st.maskFull;
                        else ++st.maskPartial;
                        ++st.maskSamples;
                    }
                }
                // Interpretacja jak w przykladzie Dissolve z SDK: 255 = zaznaczone (komentarz w PIFilter.h
                // mowi odwrotnie - rozstrzyga test wizualny Marka).
                if (ownMask) mask = m;
            }
            ProcessRect(o, (const uint8_t*)fr->inData, fr->inRowBytes, (uint8_t*)fr->outData, fr->outRowBytes, r.w(),
                        r.h(), planes, bps, offX + r.left, offY + r.top, mask, fr->maskRowBytes, p.threads);

            if (p.markTiles) {
                // Magenta na gornej i lewej krawedzi kafelka.
                auto mark = [&](int x, int y) {
                    uint8_t* px = (uint8_t*)fr->outData + (ptrdiff_t)y * fr->outRowBytes + (size_t)x * planes * bps;
                    if (bps == 1) { px[0] = 255; px[1] = 0; px[2] = 255; }
                    else { ((uint16_t*)px)[0] = (uint16_t)maxv; ((uint16_t*)px)[1] = 0; ((uint16_t*)px)[2] = (uint16_t)maxv; }
                };
                for (int x = 0; x < r.w(); ++x) mark(x, 0);
                for (int y = 1; y < r.h(); ++y) mark(0, y);
            }
            ++st.tiles;
            if (fr->progressProc) fr->progressProc(st.tiles, total);
            if (fr->abortProc && fr->abortProc()) { ZeroRects(fr); return ApplyStatus::Cancelled; }
        }
    }
    ZeroRects(fr);
    st.ms = NowMs() - t0;
    return ApplyStatus::Ok;
}

}  // namespace zk
